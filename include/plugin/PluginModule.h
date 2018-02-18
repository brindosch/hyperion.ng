#pragma once

#undef slots
#include <Python.h>
#define slots

// hyperion incl
#include <plugin/PluginDefinition.h>

// qt incl
#include <QJsonValue>

class Plugin;

class PluginModule
{
public:
	// Python 3 module def
	static struct PyModuleDef moduleDef;

	// Init module
	static PyObject* PyInit_plugin();

	// Register module once
	static void registerPluginExtensionModule();

	// json 2 python
	static PyObject* json2python		(const QJsonValue & jsonData);

	// type definition for callback enums
	static PyEnumDef callbackEnums[];
	static PyEnumDef loglvlEnums[];
	static PyEnumDef componentsEnums[];

	// Wrapper methods for Python interpreter extra buildin methods
	static PyMethodDef pluginMethods[];
	static PyObject* wrapAbort		(PyObject*, PyObject*);
	static PyObject* wrapLog		(PyObject*, PyObject* args);
	static PyObject* wrapGetSettings	(PyObject*, PyObject*);
	static PyObject* wrapGetComponentState	(PyObject*, PyObject* args);
	static PyObject* wrapSetComponentState	(PyObject*, PyObject* args);
	static PyObject* wrapSetColor		(PyObject*, PyObject* args);
	static PyObject* wrapSetEffect		(PyObject*, PyObject* args);
	static PyObject* wrapGetPriorityInfo	(PyObject*, PyObject* args);
	static PyObject* wrapGetAllPriorities	(PyObject*, PyObject*);
	static PyObject* wrapGetVisiblePriority	(PyObject*, PyObject*);
	static PyObject* wrapSetVisiblePriority	(PyObject*, PyObject* args);
	static PyObject* wrapGetBrightness	(PyObject*, PyObject* args);
	static PyObject* wrapSetBrightness	(PyObject*, PyObject* args);
	static PyObject* wrapGetAdjustmentIdList(PyObject*, PyObject*);

	// Callback methods
	static PyObject* registerCallback	(PyObject*, PyObject* args);
	static PyObject* unRegisterCallback	(PyObject*, PyObject* args);
};
