// project
#include <plugin/Plugins.h>
#include "Plugin.h"
#include <db/PluginTable.h>
#include <plugin/PluginFilesHandler.h>
#include <utils/JsonUtils.h>

// qt
#include <QThread>
#include <QTimer>

// cb thread
#include "CallbackThread.h"

// hyp
#include <hyperion/PriorityMuxer.h>

Plugins::Plugins(Hyperion* hyperion, const quint8& instance)
	: QObject(hyperion)
	, _log(Logger::getInstance("PLUGINS"))
	, _hyperion(hyperion)
	, _prioMuxer(hyperion->getMuxerInstance())
	, _PDB(new PluginTable(instance, this))
	, _pluginFilesHandler(PluginFilesHandler::getInstance())
{
	// more meta register
	qRegisterMetaType<PluginAction>("PluginAction");
	qRegisterMetaType<PluginDefinition>("PluginDefinition");
	// from PluginFilesHandler
	connect(_pluginFilesHandler, &PluginFilesHandler::pluginAction, this, &Plugins::doPluginAction);
	// to PluginFilesHandler
	connect(this, &Plugins::pluginAction, _pluginFilesHandler, &PluginFilesHandler::doPluginAction);

	// get available plugins from PluginFilesHandler and add local settings, start them if required
	const QMap<QString, PluginDefinition> instP = _pluginFilesHandler->getInstalledPlugins();
	QMap<QString, PluginDefinition>::const_iterator i = instP.constBegin();
	while (i != instP.constEnd()) {
		PluginDefinition def = i.value();
		const QString id = i.key();

		// create database entry if required
		_PDB->createPluginRecord(id);

		def.settings = _PDB->getSettings(id).toObject();
		_installedPlugins.insert(id, def);

		if(_PDB->isPluginEnabled(id))
			start(id);

	    ++i;
	}

	_pluginFilesHandler->registerMe(this);
}

Plugins::~Plugins()
{
	_pluginFilesHandler->registerMe(this);
	QMap<QString,Plugin*> copy = _runningPlugins;
	foreach (Plugin* plug, copy)
	{
		QThread* thread = plug->thread();
		plug->requestInterruption();
		// block until thread exit, if timeout is reached force the quit
		// thread->wait(5000);
		//	plug->forceExit();
	}
}

QMap<QString, PluginDefinition> Plugins::getAvailablePlugins(void)
{
	return _pluginFilesHandler->getAvailablePlugins();
}

bool Plugins::isPluginAutoUpdateEnabled(const QString& id) const
{
	return  _PDB->isPluginAutoUpdateEnabled(id);
}

void Plugins::pluginFinished()
{
	Plugin* plugin = qobject_cast<Plugin*>(sender());
	const QString id = plugin->getId();
	const PluginDefinition def = plugin->getDef();
	bool err = plugin->hasError();

	// notify error or stop
	if(err)
	{
		emit pluginAction(P_ERROR, id);
		Error(_log, "Plugin '%s' crashed",QSTRING_CSTR(def.name));
	}
	else
	{
		emit pluginAction(P_STOPPED, id);
		Info(_log, "Plugin '%s' stopped",QSTRING_CSTR(def.name));
	}

	// delete pointer and remove from list
	_runningPlugins.remove(id);
	plugin->deleteLater();

	// trigger restart (Not covered is when a plugin should be removed)
	if(_restartQueue.contains(id))
	{
		_restartQueue.removeAll(id);
		Debug(_log, "Init restart of plugin id '%s'", QSTRING_CSTR(id));
		start(id);
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
		case P_AUTOUPDATE:
			// success used to control state
			_PDB->setPluginAutoUpdateEnable(id, success);
			emit pluginAction(P_AUTOUPDATED, id);
			break;
		case P_UPD_AVAIL:
			_pluginFilesHandler->updateAvailablePlugins();
			break;
		case P_SAVE:
			saveSettings(id, def.settings);
			break;
		// final state of a install/update, remove, updated avail from Files class - forward to external
		case P_REMOVED:
			if(success)
				_installedPlugins.remove(id);
			emit pluginAction(action, id, success, def);
			break;
		case P_INSTALLED:
			emit pluginAction(action, id, success, def);
			if(success)
			{
				// create database entry if required
				_PDB->createPluginRecord(id);

				// set db timestamp
				_PDB->setPluginUpdatedAt(id);

				// read settings from db for local inject
				def.settings = _PDB->getSettings(id).toObject();
				_installedPlugins.insert(id,def);

				// we need to figure out the start after install/update
				if(id.startsWith("service.") && _PDB->isPluginEnabled(id))
					start(id);

			}
			break;
		case P_REMOVE:
		case P_SAVED:
		case P_INSTALL:
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

	if(!_installedPlugins.contains(id))
	{
		Error(_log, "Can't start plugin id '%s', it's not installed",QSTRING_CSTR(id));
		emit pluginAction(P_STARTED, id, false);
		return;
	}

	// get the definition
	PluginDefinition def = _installedPlugins.value(id);

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
		if(!_installedPlugins.contains(dep))
		{
			Error(_log,"Failed to get path of dependency %s", QSTRING_CSTR(dep));
			continue;
		}
		PluginDefinition tempDef = _installedPlugins.value(dep);
		dPaths << tempDef.entryPy;
	}

	// create plugin instance
	// set the enable state in db if required
	if(!_PDB->isPluginEnabled(id))
		_PDB->setPluginEnable(id, true);

	Plugin* newPlugin = new Plugin(this, _hyperion, def, id, dPaths);
	_runningPlugins.insert(id, newPlugin);

	QThread* pluginThread = new QThread(this);
	newPlugin->moveToThread(pluginThread);

	// plugin thread signals and plugin finished signal
	connect( pluginThread, &QThread::started, newPlugin, &Plugin::run );
	connect( newPlugin, &Plugin::finished, this, &Plugins::pluginFinished );
	connect( pluginThread, &QThread::finished, pluginThread, &QObject::deleteLater );

	// setup callback instance + thread
	CallbackThread* callbackThread = new CallbackThread();
	QThread* thread = new QThread(this);

	callbackThread->moveToThread(thread);

	connect( thread, &QThread::finished, callbackThread, &QObject::deleteLater );
	connect( thread, &QThread::finished, thread, &QObject::deleteLater );
	// kill callback thread when plugin thread finishes
	connect( newPlugin, &Plugin::finished, thread, &QThread::quit);

	// feed callback with signals
	connect(this, &Plugins::pluginAction, callbackThread, &CallbackThread::handlePluginAction);
	connect(_hyperion, &Hyperion::componentStateChanged, callbackThread, &CallbackThread::handleCompStateChanged);
	connect(_prioMuxer, &PriorityMuxer::visiblePriorityChanged, callbackThread, &CallbackThread::handleVisiblePriorityChanged);

	thread->start();

	// feed plugin with callbacks
	connect(callbackThread, &CallbackThread::onPluginAction, newPlugin, &Plugin::onPluginAction, Qt::DirectConnection);
	connect(callbackThread, &CallbackThread::onCompStateChanged, newPlugin, &Plugin::onCompStateChanged, Qt::DirectConnection);
	connect(callbackThread, &CallbackThread::onVisiblePriorityChanged, newPlugin, &Plugin::onVisiblePriorityChanged, Qt::DirectConnection);

	// notify start
	emit pluginAction(P_STARTED, id, true);
	Info(_log, "Plugin with id '%s' started",QSTRING_CSTR(id));

	// thread start
	pluginThread->start();
}

bool Plugins::stop(const QString& id, const bool& blocking) const
{
	if(_runningPlugins.contains(id))
	{
		// set the enable state in db if required
		if(_PDB->isPluginEnabled(id))
			_PDB->setPluginEnable(id, false);

		Plugin* plugin = _runningPlugins.value(id);
		QThread* pluginThread = plugin->thread();

		// catch repeated calls to prevent killTimer multiple triggers
		if(plugin->isInterruptionRequested())
			return true;

		plugin->requestInterruption();

	/*	// setup a killTimer
		QTimer::singleShot(5000, [=](){
			if(isPluginRunning(id))
				plugin->forceExit();
		});
	*/
		if(blocking)
		{
			// blocking stop is usually a remove request, so clean the queue just in case
			pluginThread->wait(6000);
		}

		return true;
	}
	return false;
}

void Plugins::saveSettings(const QString& id, const QJsonObject& settings)
{
	if(!_installedPlugins.contains(id) || id.startsWith("module."))
	{
		Error(_log,"Can't save settings for id '%s', not qualified or found.",QSTRING_CSTR(id));
		emit pluginAction(P_SAVED, id, false);
		return;
	}
	PluginDefinition newDef = _installedPlugins.value(id);
	// validate against schema
	QJsonObject schema = newDef.settingsSchema;
	if(!JsonUtils::validate("PluginSave:"+id, settings, schema, _log))
	{
		Error(_log,"Failed to validate settings for id '%s'",QSTRING_CSTR(id));
		emit pluginAction(P_SAVED, id, false);
		return;
	}
	if(_PDB->saveSettings(id, settings))
	{
		// update the definition
		newDef.settings = settings;
		_installedPlugins.insert(id, newDef);
		emit pluginAction(P_SAVED, id, true, newDef);
		return;
	}
	Error(_log,"Failed to save settings for id '%s'",QSTRING_CSTR(id));
	emit pluginAction(P_SAVED, id, false);
	return;
}
