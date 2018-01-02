#include "Plugin.h"
#include <plugin/PluginModule.h>

// hyperion
#include <hyperion/Hyperion.h>
#include <utils/Logger.h>
#include <utils/ColorRgb.h>

// qt
#include <QJsonArray>

// Get the plugin from the capsule
#define getPlugin() static_cast<Plugin*>((Plugin*)PyCapsule_Import("plugin.__pluginObj", 0))

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
	{"setComponentState",	PluginModule::wrapSetComponentState,	METH_VARARGS,	"Set a component to a state, returns false if comp is not found."},
	{"setColor",		PluginModule::wrapSetColor,		METH_VARARGS,	"Set a single color"},
	{"setEffect",		PluginModule::wrapSetEffect,		METH_VARARGS,	"Set a effect by name. Timeout and priority are optional"},

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
	// check if we have aborted already
	if (getPlugin()->isInterruptionRequested()) Py_RETURN_NONE;

	char *msg;
	int lvl;

	int argCount = PyTuple_Size(args);
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
	if (getPlugin()->isInterruptionRequested())
		Py_RETURN_NONE;
	else
		return Py_BuildValue("O",PluginModule::json2python(getPlugin()->getSettings()));
}

PyObject* PluginModule::wrapSetComponentState(PyObject *, PyObject *args)
{
	// check if we have aborted already
	if (getPlugin()->isInterruptionRequested()) Py_RETURN_NONE;

	int comp, enable;

	// log with lvl
	if (!PyArg_ParseTuple(args, "ii", &comp, &enable))
	{
		PyErr_SetString(PyExc_TypeError, "To set a component state, you need two int args");
		return nullptr;;
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

	int argCount = PyTuple_Size(args);
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
	return nullptr;;
}

PyObject* PluginModule::wrapSetEffect(PyObject *, PyObject *args)
{
	// check if we have aborted already
	if (getPlugin()->isInterruptionRequested()) Py_RETURN_NONE;

	char* name;
	int duration = -1;
	int priority = 50;

	int argCount = PyTuple_Size(args);
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

PyObject *PluginModule::registerCallback(PyObject *, PyObject *args)
{
	// check if we have aborted already
	if (getPlugin()->isInterruptionRequested()) Py_RETURN_NONE;

	PyObject *new_callback = nullptr;
	int callback_type;

	if(PyTuple_Size(args) == 2 && PyArg_ParseTuple(args, "iO:registerCallback", &callback_type, &new_callback))
	{
		if (!PyCallable_Check(new_callback))
		{
			PyErr_SetString(PyExc_TypeError, "second parameter must be callable");
			return nullptr;
		}

		Py_XINCREF(new_callback); // Add a reference to new callback

		switch(callback_type)
		{
			case ON_SETTINGS_CHANGED:
			case ON_COMP_STATE_CHANGED:
				getPlugin()->callbackObjects.insert(callbackTypeToString(callback_type), new_callback);
				break;
			default:
				PyErr_SetString(PyExc_TypeError, "first parameter must be an callback enum");
				return nullptr;
		}
	}

	Py_XDECREF(new_callback);
	Py_RETURN_NONE;
}

PyObject *PluginModule::unRegisterCallback(PyObject *, PyObject *args)
{
	// check if we have aborted already
	if (getPlugin()->isInterruptionRequested()) Py_RETURN_NONE;

	int callback_type;
	if(PyArg_ParseTuple(args, "i:unregisterCallback", &callback_type))
	{
		switch(callback_type)
		{
			case ON_SETTINGS_CHANGED:
			case ON_COMP_STATE_CHANGED:
				getPlugin()->callbackObjects.remove(callbackTypeToString(callback_type));
				break;
			default:
				PyErr_SetString(PyExc_TypeError, "callback enum required");
				return nullptr;
		}
	}

	Py_RETURN_NONE;
}
