// project
#include <plugin/Plugins.h>
#include "Plugin.h"
#include <db/PluginTable.h>

Plugins::Plugins(Hyperion* hyperion)
	: QObject()
	, _log(Logger::getInstance("PLUGINS"))
	, _hyperion(hyperion)
	, _PDB(new PluginTable(_hyperion->getConfigFileName()))
	, _files(_hyperion->getRootPath(), _hyperion->getConfigFileName(), _PDB)
{
	// from files
	connect(&_files, &Files::pluginAction, this, &Plugins::doPluginAction);
	// to files
	connect(this, &Plugins::pluginAction, &_files, &Files::doPluginAction);

	// file handling init after database creation and signal link
	_files.init();
}

Plugins::~Plugins()
{
	foreach (Plugin* plug, _runningPlugins)
	    plug->requestInterruption();
	delete _PDB;
}

bool Plugins::isPluginAutoUpdateEnabled(const QString& id) const
{
	return  _PDB->isPluginAutoUpdateEnabled(id);
}

void Plugins::pluginFinished()
{
	Plugin* plugin = qobject_cast<Plugin*>(sender());
	const QString id = plugin->getId();
	bool err = plugin->hasError();

	// notify error or stop
	if(err)
	{
		emit pluginAction(P_ERROR, id);
		Error(_log, "Plugin with id '%s' crashed",QSTRING_CSTR(id));
	}
	else
	{
		emit pluginAction(P_STOPPED, id);
		Info(_log, "Plugin with id '%s' stopped",QSTRING_CSTR(id));
	}

	// remove plugin from fs if requested
	const bool& rem = plugin->hasRemoveFlag();
	if(rem)
		_files.removePlugin(id);

	// delete pointer and remove from list
	_runningPlugins.remove(id);
	plugin->deleteLater();

	// trigger restart
	if(_restartQueue.contains(id))
	{
		_restartQueue.removeAll(id);
		// not if the plugin should be removed
		if(!rem)
		{
			Debug(_log, "Init restart of plugin id '%s'", QSTRING_CSTR(id));
			start(id);
		}
	}
}

void Plugins::doPluginAction(PluginAction action, QString id, bool success, PluginDefinition def)
{
	switch(action)
	{
		case P_START:
			start(id);
			break;
		case P_STOP:
			if(!stop(id))
				emit pluginAction(P_STOPPED, id, false);
			break;
		case P_REMOVE:
			// check if plugin is running
			if(!stop(id, true))
				_files.removePlugin(id);
			break;
		case P_AUTOUPDATE:
			// success used to control state
			_PDB->setPluginAutoUpdateEnable(id, success);
			emit pluginAction(P_AUTOUPDATED, id);
			break;
		case P_UPD_AVAIL:
			_files.updateAvailablePlugins();
			break;
		case P_SAVE:
			_files.saveSettings(id, def.settings);
			break;
		// final state of a install/update, remove, updated avail from Files class - forward to external
		case P_SAVED:
		case P_INSTALL:
		case P_INSTALLED:
		case P_REMOVED:
		case P_UPDATED_AVAIL:
			emit pluginAction(action, id, success, def);
			break;
		default:
			break;
	}
}

void Plugins::start(QString id)
{
	if(!id.startsWith("service."))
	{
		Error(_log, "Can't start plugin id '%s', it's not meant to be started",QSTRING_CSTR(id));
		emit pluginAction(P_STARTED, id, false);
		return;
	}

	// get the definition
	PluginDefinition def;
	if(!_files.getPluginDefinition(id, def))
	{
		Error(_log, "Can't start plugin id '%s', it's not installed",QSTRING_CSTR(id));
		emit pluginAction(P_STARTED, id, false);
		return;
	}

	// if plugin id is currently running stop it and add to _restartQueue
	if(_runningPlugins.contains(id))
	{
		Plugin* plugin = _runningPlugins.value(id);

		// protect queue of repeated entrys
		if(plugin->isInterruptionRequested())
			return;

		_restartQueue << id;
		plugin->requestInterruption();
		return;
	}

	// extract dependencies paths
	QStringList dPaths;
	QJsonObject deps = def.dependencies;
	deps.remove("hyperion");
	for (QJsonObject::iterator it = deps.begin(); it != deps.end(); it++)
	{
		QString dep = it.key();
		PluginDefinition tempDef;
		if(!_files.getPluginDefinition(dep, tempDef))
		{
			Error(_log,"Failed to get path of dependency %s", QSTRING_CSTR(dep));
			continue;
		}
		dPaths << tempDef.entryPy;
	}

	// create plugin instance
	// set the enable state in db if required
	if(!_PDB->isPluginEnabled(id))
		_PDB->setPluginEnable(id, true);

	Plugin* newPlugin = new Plugin(this, _hyperion, def, id, dPaths);
	_runningPlugins.insert(id, newPlugin);

	// notify
	emit pluginAction(P_STARTED, id, true);
	Info(_log, "Plugin with id '%s' started",QSTRING_CSTR(id));

	// listen for plugin thread exit
	connect(newPlugin, &QThread::finished, this, &Plugins::pluginFinished);
	// listen for pluginActions
	connect(this, &Plugins::pluginAction, newPlugin, &Plugin::handlePluginAction);

	// listen for componentStateChanged
	qRegisterMetaType<hyperion::Components>("hyperion::Components");
	connect(_hyperion, SIGNAL(componentStateChanged(hyperion::Components,bool)), newPlugin, SLOT(onCompStateChanged(hyperion::Components,bool)));

	// start
	newPlugin->start();
}

bool Plugins::stop(const QString& id, const bool& remove) const
{
	if(_runningPlugins.contains(id))
	{
		// set the enable state in db if required
		if(_PDB->isPluginEnabled(id))
			_PDB->setPluginEnable(id, false);

		Plugin* plugin = _runningPlugins.value(id);

		if(remove)
			plugin->setRemoveFlag();

		plugin->requestInterruption();
		plugin->wait();
		return true;
	}
	return false;
}
