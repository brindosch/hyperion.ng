#include "Plugin.h"
#include <plugin/PluginModule.h>

// hyperion
#include <hyperion/Hyperion.h>
#include <utils/Logger.h>
#include <utils/ColorRgb.h>
#include <hyperion/PriorityMuxer.h>

// qt
#include <QJsonArray>
#include <QDateTime>

// Get the plugin from the capsule
#define getPlugin() static_cast<Plugin*>((Plugin*)PyCapsule_Import("plugin.__pluginObj", 0))

PyEnumDef PluginModule::callbackEnums[]  = {
	{ (char*) "ON_COMP_STATE_CHANGED",		CallbackAction::ON_COMP_STATE_CHANGED		},
	{ (char*) "ON_SETTINGS_CHANGED",		CallbackAction::ON_SETTINGS_CHANGED		},
	{ (char*) "ON_VISIBLE_PRIORITY_CHANGED",	CallbackAction::ON_VISIBLE_PRIORITY_CHANGED	},
	{ nullptr, 0}
};

PyEnumDef PluginModule::loglvlEnums[]  = {
	{ (char*) "LOG_INFO",		90	},
	{ (char*) "LOG_WARNING",	91	},
	{ (char*) "LOG_ERROR",		92	},
	{ (char*) "LOG_DEBUG",		93	},
	{ nullptr, 0}
};

PyEnumDef PluginModule::componentsEnums[] = {
	{ (char*) "COMP_ALL",				hyperion::Components::COMP_ALL			},
	{ (char*) "COMP_SMOOTHING",			hyperion::Components::COMP_SMOOTHING		},
	{ (char*) "COMP_BLACKBORDER",			hyperion::Components::COMP_BLACKBORDER		},
	{ (char*) "COMP_LEDDEVICE",			hyperion::Components::COMP_LEDDEVICE		},
	{ (char*) "COMP_GRABBER",			hyperion::Components::COMP_GRABBER		},
	{ (char*) "COMP_V4L",				hyperion::Components::COMP_V4L			},
/*	{ (char*) "COMP_FORWARDER",			hyperion::Components::COMP_FORWARDER		},
	{ (char*) "COMP_UDPLISTENER",			hyperion::Components::COMP_UDPLISTENER		},
	{ (char*) "COMP_BOBLIGHTSERVER",		hyperion::Components::COMP_BOBLIGHTSERVER	},
	{ (char*) "COMP_COLOR",				hyperion::Components::COMP_COLOR		},
	{ (char*) "COMP_IMAGE",				hyperion::Components::COMP_IMAGE		},
	{ (char*) "COMP_EFFECT",			hyperion::Components::COMP_EFFECT		},
	{ (char*) "COMP_PROTOSERVER",			hyperion::Components::COMP_PROTOSERVER		},
*/	{ nullptr, 0}
};

struct PyModuleDef PluginModule::moduleDef = {
	PyModuleDef_HEAD_INIT,		/* m_base */
	"plugin",			/* m_name */
	"Plugin module",		/* m_doc */
	-1,				/* m_size */
	PluginModule::pluginMethods,	/* m_methods */
	nullptr,			/* m_reload */
	nullptr,			/* m_traverse */
	nullptr,			/* m_clear */
	nullptr,			/* m_free */
};

// Python method table
PyMethodDef PluginModule::pluginMethods[] = {
	{"log",			PluginModule::wrapLog,			METH_VARARGS,	"Write a message to the log"},
	{"abort",		PluginModule::wrapAbort,		METH_NOARGS,	"Check if the plugin should abort execution."},
	{"getSettings",		PluginModule::wrapGetSettings,		METH_NOARGS,	"Get the settings object"},
	{"getComponentState",	PluginModule::wrapGetComponentState,	METH_VARARGS,	"Get the component state of a specific component"},
	{"setComponentState",	PluginModule::wrapSetComponentState,	METH_VARARGS,	"Set a component to a state, returns false if comp is not found."},
	{"setColor",		PluginModule::wrapSetColor,		METH_VARARGS,	"Set a single color"},
	{"setEffect",		PluginModule::wrapSetEffect,		METH_VARARGS,	"Set a effect by name. Timeout and priority are optional"},
	{"getPriorityInfo",	PluginModule::wrapGetPriorityInfo,	METH_VARARGS,	"Get the priority info for given priority"},
	{"getAllPriorities",	PluginModule::wrapGetAllPriorities,	METH_NOARGS,	"Get all registered priorities from Priority Muxer"},
	{"getVisiblePriority",	PluginModule::wrapGetVisiblePriority,	METH_NOARGS,	"Get the current visible priority"},
	{"setVisiblePriority",	PluginModule::wrapSetVisiblePriority,	METH_VARARGS,	"Select a specific priority"},
	{"setBrightness",	PluginModule::wrapSetBrightness,	METH_VARARGS,	"Set the led brightness by default value or specific identifier"},
	{"getBrightness",	PluginModule::wrapGetBrightness,	METH_VARARGS,	"Get the led brightness by default value or specific identifier"},
	{"getAdjustmentList",	PluginModule::wrapGetAdjustmentIdList,	METH_NOARGS,	"Returns a list with unique adjustment identifiers"},

	// callback methods
	{"registerCallback",	PluginModule::registerCallback,		METH_VARARGS,	"Register a callback function."},
	{"unregisterCallback",	PluginModule::unRegisterCallback,	METH_VARARGS,	"Unregister a callback function."},
	{nullptr, nullptr, 0, nullptr}
};

PyObject* PluginModule::PyInit_plugin()
{
	return PyModule_Create(&moduleDef);
}

void PluginModule::registerPluginExtensionModule()
{
	PyImport_AppendInittab("plugin", &PyInit_plugin);
}

PyObject *PluginModule::json2python(const QJsonValue &jsonData)
{
	switch (jsonData.type())
	{
		case QJsonValue::Null:
			Py_RETURN_NONE;
		case QJsonValue::Undefined:
			Py_RETURN_NOTIMPLEMENTED;
		case QJsonValue::Double:
		{
			if (std::rint(jsonData.toDouble()) != jsonData.toDouble())
			{
				return Py_BuildValue("d", jsonData.toDouble());
			}
			return Py_BuildValue("i", jsonData.toInt());
		}
		case QJsonValue::Bool:
			return Py_BuildValue("i", jsonData.toBool() ? 1 : 0);
		case QJsonValue::String:
			return Py_BuildValue("s", jsonData.toString().toUtf8().constData());
		case QJsonValue::Object:
		{
			PyObject * dict= PyDict_New();
			QJsonObject objectData = jsonData.toObject();
			for (QJsonObject::iterator i = objectData.begin(); i != objectData.end(); ++i)
			{
				PyObject * obj = json2python(*i);
				PyDict_SetItemString(dict, i.key().toStdString().c_str(), obj);
				Py_XDECREF(obj);
			}
			return dict;
		}
		case QJsonValue::Array:
		{
			QJsonArray arrayData = jsonData.toArray();
			PyObject * list = PyList_New(arrayData.size());
			int index = 0;
			for (QJsonArray::iterator i = arrayData.begin(); i != arrayData.end(); ++i, ++index)
			{
				PyObject * obj = json2python(*i);
				Py_INCREF(obj);
				PyList_SetItem(list, index, obj);
				Py_XDECREF(obj);
			}
			return list;
		}
	}

	assert(false);
	Py_RETURN_NONE;
}

PyObject* PluginModule::wrapAbort(PyObject *, PyObject *)
{
	return Py_BuildValue("i", getPlugin()->isInterruptionRequested() ? 1 : 0);
}

PyObject* PluginModule::wrapLog(PyObject *, PyObject *args)
{
	char *msg;
	int lvl;

	Py_ssize_t argCount = PyTuple_Size(args);
	if (argCount == 2)
	{
		// log with lvl
		if (!PyArg_ParseTuple(args, "si", &msg, &lvl))
		{
			PyErr_SetString(PyExc_TypeError, "String as message and lvl as integer required");
			return nullptr;
		}
		getPlugin()->printToLog(msg, lvl);
		Py_RETURN_NONE;
	}
	else if (argCount == 1)
	{
		if(!PyArg_ParseTuple(args, "s", &msg))
		{
			PyErr_SetString(PyExc_TypeError, "String required to print a log message");
			return nullptr;
		}
		getPlugin()->printToLog(msg);
		Py_RETURN_NONE;
	}
	Py_RETURN_NONE;
}

PyObject* PluginModule::wrapGetSettings(PyObject *, PyObject *)
{
	// check if we have aborted already
	if (getPlugin()->isInterruptionRequested()) Py_RETURN_NONE;
	return Py_BuildValue("O", PluginModule::json2python(getPlugin()->getSettings()));
}

PyObject* PluginModule::wrapGetComponentState(PyObject *, PyObject *args)
{
	// check if we have aborted already
	if (getPlugin()->isInterruptionRequested()) Py_RETURN_NONE;

	int comp;

	// log with lvl
	if (!PyArg_ParseTuple(args, "i", &comp))
	{
		PyErr_SetString(PyExc_TypeError, "int argument required to getComponentState()");
		return nullptr;
	}
	return Py_BuildValue("i",getPlugin()->getComponentState(comp));
}

PyObject* PluginModule::wrapSetComponentState(PyObject *, PyObject *args)
{
	// check if we have aborted already
	if (getPlugin()->isInterruptionRequested()) Py_RETURN_NONE;

	int comp, enable;

	if (!PyArg_ParseTuple(args, "ii", &comp, &enable))
	{
		PyErr_SetString(PyExc_TypeError, "To setComponentState(), you need two int args");
		return nullptr;
	}
	return Py_BuildValue("i",getPlugin()->setComponentState(comp, enable));
}

PyObject* PluginModule::wrapSetColor(PyObject *, PyObject *args)
{
	// check if we have aborted already
	if (getPlugin()->isInterruptionRequested()) Py_RETURN_NONE;

	ColorRgb color;
	int duration = -1;
	int priority = 50;

	Py_ssize_t argCount = PyTuple_Size(args);
	if(argCount == 3 && PyArg_ParseTuple(args, "bbb", &color.red, &color.green, &color.blue))
	{
		getPlugin()->setColor(color, priority, duration);
		Py_RETURN_NONE;
	}
	else if(argCount == 4 && PyArg_ParseTuple(args, "bbbi", &color.red, &color.green, &color.blue, &duration))
	{
		getPlugin()->setColor(color, priority, duration);
		Py_RETURN_NONE;
	}
	else if(argCount == 5 && PyArg_ParseTuple(args, "bbbii", &color.red, &color.green, &color.blue, &duration, &priority))
	{
		getPlugin()->setColor(color, priority, duration);
		Py_RETURN_NONE;
	}
	PyErr_SetString(PyExc_RuntimeError, "Invalid arguments for setColor()");
	return nullptr;
}

PyObject* PluginModule::wrapSetEffect(PyObject *, PyObject *args)
{
	// check if we have aborted already
	if (getPlugin()->isInterruptionRequested()) Py_RETURN_NONE;

	char* name;
	int duration = -1;
	int priority = 50;

	Py_ssize_t argCount = PyTuple_Size(args);
	if(argCount == 1 && PyArg_ParseTuple(args, "s", &name))
	{
		return Py_BuildValue("i", getPlugin()->setEffect(name, priority, duration));
	}
	else if(argCount == 2 && PyArg_ParseTuple(args, "si", &name, &duration))
	{
		return Py_BuildValue("i", getPlugin()->setEffect(name, priority, duration));
	}
	else if(argCount == 3 && PyArg_ParseTuple(args, "sii", &name, &duration, &priority))
	{
		return Py_BuildValue("i", getPlugin()->setEffect(name, priority, duration));
	}
	PyErr_SetString(PyExc_RuntimeError, "Invalid arguments for setEffect()");
	return nullptr;
}

PyObject* PluginModule::wrapGetPriorityInfo(PyObject *, PyObject *args)
{
	// check if we have aborted already
	if (getPlugin()->isInterruptionRequested()) Py_RETURN_NONE;

	int priority;

	Py_ssize_t argCount = PyTuple_Size(args);
	if(argCount == 1 && PyArg_ParseTuple(args, "i", &priority))
	{
		const PriorityMuxer::InputInfo info = getPlugin()->getPriorityInfo(priority);
		int timeout = (info.timeoutTime_ms > 0) ? (QDateTime::currentMSecsSinceEpoch() - info.timeoutTime_ms) : info.timeoutTime_ms;
		return Py_BuildValue("{s:i,s:i,s:s,s:s,s:s}"
			, "priority", info.priority
			, "timeout", timeout
			, "component" , QSTRING_CSTR(info.origin)
			, "origin" , componentToIdString(info.componentId)
		 	, "owner" , QSTRING_CSTR(info.owner) );
	}
	PyErr_SetString(PyExc_RuntimeError, "Invalid argument for getPriorityInfo()");
	return nullptr;
}

PyObject* PluginModule::wrapGetAllPriorities(PyObject *, PyObject *args)
{
	// check if we have aborted already
	if (getPlugin()->isInterruptionRequested()) Py_RETURN_NONE;

	const QList<int> prioList = getPlugin()->getAllPriorities();
	PyObject* result = PyList_New(prioList.size());

	for(int i = 0; i < prioList.size(); ++i)
		PyList_SET_ITEM(result, i, Py_BuildValue("i", prioList.at(i)));

	return result;
}

PyObject* PluginModule::wrapGetVisiblePriority(PyObject *, PyObject *args)
{
	// check if we have aborted already
	if (getPlugin()->isInterruptionRequested()) Py_RETURN_NONE;
	return Py_BuildValue("i", getPlugin()->getVisiblePriority());
}

PyObject* PluginModule::wrapSetVisiblePriority(PyObject *, PyObject *args)
{
	// check if we have aborted already
	if (getPlugin()->isInterruptionRequested()) Py_RETURN_NONE;

	int priority;

	Py_ssize_t argCount = PyTuple_Size(args);
	if(argCount == 1 && PyArg_ParseTuple(args, "i", &priority))
		return Py_BuildValue("i", getPlugin()->setVisiblePriority(priority));

	PyErr_SetString(PyExc_RuntimeError, "Invalid argument for setPriority()");
	return nullptr;
}

PyObject* PluginModule::wrapGetBrightness(PyObject *, PyObject *args)
{
	// check if we have aborted already
	if (getPlugin()->isInterruptionRequested()) Py_RETURN_NONE;

	char *id = nullptr;
	if(PyArg_ParseTuple(args, "|s:getBrightness", &id) && id)
		return Py_BuildValue("i", getPlugin()->getBrightness(QString(id)));

	return Py_BuildValue("i", getPlugin()->getBrightness());
}

PyObject* PluginModule::wrapSetBrightness(PyObject *, PyObject *args)
{
	// check if we have aborted already
	if (getPlugin()->isInterruptionRequested()) Py_RETURN_NONE;

	int brightness;
	char *id = nullptr;

	if(!(PyTuple_Size(args) < 1) && PyArg_ParseTuple(args, "i|s:setBrightness", &brightness, &id))
	{
		if (!(brightness < 0) && (brightness <= 100))
		{
			if (!id)
				return Py_BuildValue("i", getPlugin()->setBrightness(brightness));
			else
				return Py_BuildValue("i", getPlugin()->setBrightness(brightness, QString(id)));
		}
		
		PyErr_SetString(PyExc_RuntimeError, "Invalid brightness value.");
		return nullptr;
	}

	PyErr_SetString(PyExc_RuntimeError, "Invalid argument for setBrightness()");
	return nullptr;
}

PyObject* PluginModule::wrapGetAdjustmentIdList(PyObject *, PyObject *)
{
	// check if we have aborted already
	if (getPlugin()->isInterruptionRequested()) Py_RETURN_NONE;

	const QStringList idList = getPlugin()->getAdjustmentIdList();
	PyObject* result = PyList_New(idList.size());

	for (int i = 0; i < idList.size(); ++i)
		PyList_SET_ITEM(result, i, Py_BuildValue("s", idList.at(i).toUtf8().constData()));

	return result;
}

PyObject *PluginModule::registerCallback(PyObject *, PyObject *args)
{
	// check if we have aborted already
	if (getPlugin()->isInterruptionRequested()) Py_RETURN_NONE;

	PyObject* new_callback, *optional_eventList = nullptr;
	int callback_type;

	// the |O! parses for an optional python object (optional_eventList) checked  to be of type PyList_Type
	if(PyArg_ParseTuple(args, "iO|O!:registerCallback", &callback_type, &new_callback, &(PyList_Type), &optional_eventList))
	{
		if (!PyCallable_Check(new_callback))
		{
			PyErr_SetString(PyExc_TypeError, "second parameter must be callable");
			return nullptr;
		}

		Py_XINCREF(new_callback); // add a reference to new callback

		// if callbackObjects contains the callback adress given by new_callback, remove it first.
		auto it = getPlugin()->callbackObjects.begin();
		while (it != getPlugin()->callbackObjects.end())
		{
			if ((PyList_Check(it.value()) && new_callback == PyList_GetItem(it.value(), 0)) || (it.value() == new_callback))
			{
				getPlugin()->callbackObjects.erase(it);
				break;
			}
			++it;
		}

		switch(callback_type)
		{
			case CallbackAction::ON_SETTINGS_CHANGED:
			case CallbackAction::ON_COMP_STATE_CHANGED:
			case CallbackAction::ON_VISIBLE_PRIORITY_CHANGED:
			{
				// no specific events
				if (!optional_eventList)
				{
					getPlugin()->callbackObjects.insertMulti(PluginModule::callbackEnums[callback_type].name, new_callback); // register the callback
					break;
				}
				// may contain events
				else
				{
					PyObject *iter = PyObject_GetIter(optional_eventList); // get an iterator over the eventlist
					if (iter)
					{
						PyObject *pyListObject = PyList_New(1); // create PyListObject
						PyList_SetItem(pyListObject, 0, new_callback); // steals reference to new_callback

						while (true) // run through the eventlist
						{
							PyObject *event = PyIter_Next(iter);

							if (!event)
								break; // nothing left in the iterator

							if (!PyLong_Check(event))
								continue; // skip it, we were expecting an event enum

							PyList_Append(pyListObject, event); // Add the event to the eventlist
							Py_DECREF(event);
						}

						getPlugin()->callbackObjects.insertMulti(PluginModule::callbackEnums[callback_type].name, PyList_Size(pyListObject) > 1 ? pyListObject : new_callback); // register the callback
					}
					// It's not iterable.
					else
						getPlugin()->callbackObjects.insertMulti(PluginModule::callbackEnums[callback_type].name, new_callback); // register the callback
				}
				break;
			}
			default:
				PyErr_SetString(PyExc_TypeError, "first parameter must be an callback enum");
				return nullptr;
		}
	}

	Py_XDECREF(new_callback);
	Py_XDECREF(optional_eventList);
	Py_RETURN_NONE;
}

PyObject *PluginModule::unRegisterCallback(PyObject *, PyObject *args)
{
	// check if we have aborted already
	if (getPlugin()->isInterruptionRequested()) Py_RETURN_NONE;

	PyObject *registered_callback = nullptr;
	if(PyArg_ParseTuple(args, "O:unregisterCallback", &registered_callback))
{
		if (!PyCallable_Check(registered_callback))
		{
			PyErr_SetString(PyExc_TypeError, "first parameter must be callable");
			return nullptr;
		}

		auto it = getPlugin()->callbackObjects.begin();
		while (it != getPlugin()->callbackObjects.end())
		{
			if ((PyList_Check(it.value()) && registered_callback == PyList_GetItem(it.value(), 0)) || ( it.value() == registered_callback))
			{
				getPlugin()->callbackObjects.erase(it);
				break;
			}
			++it;
		}
	}

	Py_XDECREF(registered_callback);
	Py_RETURN_NONE;
}
