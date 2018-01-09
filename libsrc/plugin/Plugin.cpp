// proj include
#include "Plugin.h"
#include <plugin/PluginModule.h>
#include <plugin/Plugins.h>

// hyperion
#include <hyperion/Hyperion.h>
#include <utils/Components.h>

// python utils/ global mainthread
#include <python/PythonUtils.h>

Plugin::Plugin(Plugins* plugins, Hyperion * hyperion, const PluginDefinition& def, const QString& id,  const QStringList& dPaths)
	: QThread()
	, _plugins(plugins)
	, _hyperion(hyperion)
	, _prioMuxer(hyperion->getMuxerInstance())
	, _def(def)
	, _id(id)
	, _dPaths(dPaths)
	, _log(Logger::getInstance("PLUGIN"))
{
	// prio mixer signal
	connect(_prioMuxer, &PriorityMuxer::visiblePriorityChanged, this, &Plugin::onVisiblePriorityChanged, Qt::QueuedConnection);
}

Plugin::~Plugin()
{
	// removes all Callbacks
	callbackObjects.clear();
}

void Plugin::run()
{
	// gil lock
	PyEval_RestoreThread(mainThreadState);
	// Initialize a new  sub-interpreter
	_state = Py_NewInterpreter();
	// verify we got a thread state
	if (_state == nullptr)
	{
		// Release the GIL and reset the thread state to NULL, returning the previous thread state
		PyEval_ReleaseLock();
		Error(_log, "No thread state for id %s!",QSTRING_CSTR(_id));
		_error = true;
		return;
	}
	// swap interpreter thread
	PyThreadState_Swap(_state);

	// import the buildtin plugin module
	PyObject * module = PyImport_ImportModule("plugin");

	// add a capsule containing 'this' to the dict to be able to retrieve the effect from the callback function
	PyModule_AddObject(module, "__pluginObj", PyCapsule_New((void*)this, "plugin.__pluginObj", nullptr));

	// for callback and components enums add an integer constant to module as name
	// hint: the integer constants for the components enum is added with the number of elements in callback, to not conflict with the callback enums
	for (PyEnumDef* callback = PluginModule::callbackEnums; callback->name != NULL; callback++)
		PyModule_AddIntConstant(module, callback->name, callback->value);

	for (PyEnumDef* components = PluginModule::componentsEnums; components->name != NULL; components++)
		PyModule_AddIntConstant(module, components->name, components->value + 3);

	// decref the module
	Py_XDECREF(module);

	// create and apply Python path with dependencies
	handlePyPath();

	PyObject *ioMod, *openedFile;
	ioMod = PyImport_ImportModule("io");
	openedFile = PyObject_CallMethod(ioMod, "open", "ss", (char *)QSTRING_CSTR(_def.entryPy), "r");
	Py_DECREF(ioMod);
	FILE *fp = PyFile_AsFileWithMode(openedFile, (char *)"r");

	if (fp == nullptr)
	{
		Error(_log, "ID %s: Failed to open script '%s'", QSTRING_CSTR(_id), QSTRING_CSTR(_def.entryPy));
		_error = true;
	}
	else
	{
		PyObject *f = PyUnicode_FromString(QSTRING_CSTR(_def.entryPy)); // New ref
		//PyDict_SetItemString(moduleDict, "__file__", f);
		//onPythonModuleInitialization(moduleDict);
		Py_DECREF(f);

		PyObject *main_module = PyImport_ImportModule("__main__"); // New Reference
		PyObject *main_dict = PyModule_GetDict(main_module); // Borrowed reference
		Py_INCREF(main_dict); // Incref "main_dict" to use it in PyRun_File(), because PyModule_GetDict() has decref "main_dict"
		Py_DECREF(main_module); // // release "main_module"

		// run
		PyObject* result = PyRun_File(fp, QSTRING_CSTR(_def.entryPy), Py_file_input, main_dict, main_dict);  // New Reference, may be NULL

		// handle exception
		if (!result && PyErr_Occurred()) // borrowed
		{
			printException();
		}
		Py_XDECREF(result);  // release "result"
		Py_DECREF(main_dict);  // release "main_dict"
	}

	// make sure all sub threads have finished
	for (PyThreadState* s = _state->interp->tstate_head, *old = nullptr; s;)
	{
		if (s == _state)
		{
			s = s->next;
			continue;
		}
		if (old != s)
		{
  			Debug(_log,"ID %s: Waiting on thread %u", QSTRING_CSTR(_id), s->thread_id);
			old = s;
		}

		Py_BEGIN_ALLOW_THREADS;
		msleep(100);
		Py_END_ALLOW_THREADS;

		s = _state->interp->tstate_head;
	}

	Py_EndInterpreter(_state);
	PyEval_ReleaseLock();
}

void Plugin::handlePyPath(void)
{
	// get current paths, prefer sys.path over Py_GetPath
	PyObject *sysMod(PyImport_ImportModule((char*)"sys")); // new ref
	PyObject *sysModDict(PyModule_GetDict(sysMod)); // borrowed ref
	PyObject *pathObj(PyDict_GetItemString(sysModDict, "path")); // borrowed ref

	if (pathObj != NULL && PyList_Check(pathObj))
	{
		for (int i = 0; i < PyList_Size(pathObj); i++)
		{
			PyObject *e = PyList_GetItem(pathObj, i); // borrowed ref
			if (e != NULL && PyUnicode_Check(e))
				addNativePath(PyUnicode_AsUTF8(e));
		}
	}
	else
		addNativePath(QString::fromWCharArray(Py_GetPath()).toStdString());

	Py_DECREF(sysMod);

	// add dependencies to _pythonPath
	for(const auto entry : _dPaths)
	{
		addNativePath(entry.toStdString());
	}

	// apply path
	Debug(_log,"ID %s: Python path: %s",QSTRING_CSTR(_id),_pythonPath.c_str());
	PySys_SetPath(QString::fromStdString(_pythonPath).toStdWString().c_str());
}

void Plugin::addNativePath(const std::string& path)
{
  if (path.empty())
    return;

  if (!_pythonPath.empty())
    _pythonPath += PY_PATH_SEP;

  _pythonPath += path;
}

void Plugin::printException(void)
{
	_error = true;

	Error(_log,"###### PYTHON EXCEPTION ######");
	Error(_log,"## In plugin '%s' id '%s'", QSTRING_CSTR(_def.name), QSTRING_CSTR(_id));
	/* Objects all initialized to NULL for Py_XDECREF */
	PyObject *errorType = nullptr, *errorValue = nullptr, *errorTraceback = nullptr;

	PyErr_Fetch(&errorType, &errorValue, &errorTraceback); // New Reference or NULL
	PyErr_NormalizeException(&errorType, &errorValue, &errorTraceback);

	// Extract exception message from "errorValue"
	if(errorValue)
	{
		QString message;
		if(PyObject_HasAttrString(errorValue, "__class__"))
		{
			PyObject *classPtr = PyObject_GetAttrString(errorValue, "__class__"); // New Reference
			PyObject *class_name = nullptr; // Object "class_name" initialized to NULL for Py_XDECREF
			class_name = PyObject_GetAttrString(classPtr, "__name__"); // New Reference or NULL

			if(class_name && PyUnicode_Check(class_name))
				message.append(PyUnicode_AsUTF8(class_name));

			Py_DECREF(classPtr); // release "classPtr" when done
			Py_XDECREF(class_name); // Use Py_XDECREF() to ignore NULL references
		}

		PyObject *valueString = nullptr;
		valueString = PyObject_Str(errorValue); // New Reference or NULL

		if(valueString && PyUnicode_Check(valueString))
		{
			if(!message.isEmpty())
				message.append(": ");

			message.append(PyUnicode_AsUTF8(valueString));
		}
		Py_XDECREF(valueString); // Use Py_XDECREF() to ignore NULL references

		Error(_log, "## %s", QSTRING_CSTR(message));
	}

	// Extract exception message from "errorTraceback"
	if(errorTraceback)
	{
		// Object "tracebackList" initialized to NULL for Py_XDECREF
		PyObject *tracebackModule = nullptr, *methodName = nullptr, *tracebackList = nullptr;

		tracebackModule = PyImport_ImportModule("traceback"); // New Reference or NULL
		methodName = PyUnicode_FromString("format_exception"); // New Reference or NULL
		tracebackList = PyObject_CallMethodObjArgs(tracebackModule, methodName, errorType, errorValue, errorTraceback, NULL); // New Reference or NULL

		if(tracebackList)
		{
			PyObject* iterator = PyObject_GetIter(tracebackList); // New Reference

			PyObject* item;
			while( (item = PyIter_Next(iterator)) ) // New Reference
			{
				Error(_log, "## %s", QSTRING_CSTR(QString(PyUnicode_AsUTF8(item)).trimmed()));
				Py_DECREF(item); // release "item" when done
			}
			Py_DECREF(iterator);  // release "iterator" when done
		}

		// Use Py_XDECREF() to ignore NULL references
		Py_XDECREF(tracebackModule);
		Py_XDECREF(methodName);
		Py_XDECREF(tracebackList);

		// Give the exception back to python and print it to stderr in case anyone else wants it.
		Py_XINCREF(errorType);
		Py_XINCREF(errorValue);
		Py_XINCREF(errorTraceback);

		PyErr_Restore(errorType, errorValue, errorTraceback);
		// PyErr_PrintEx(0); // Uncomment this line to switch on stderr output
	}
	Error(_log,"###### EXCEPTION END ######");
}

FILE* Plugin::PyFile_AsFileWithMode(PyObject *py_file, const char *mode)
{
    FILE *f;
    PyObject *ret;
    int fd;

    ret = PyObject_CallMethod(py_file, "flush", "");
    if (ret == NULL)
        return NULL;
    Py_DECREF(ret);

    fd = PyObject_AsFileDescriptor(py_file);
    if (fd == -1)
        return NULL;

    f = fdopen(fd, mode);
    if (f == NULL) {
        PyErr_SetFromErrno(PyExc_OSError);
		return NULL;
	}

    return f;
}

void Plugin::printToLog(char* msg, int lvl)
{
	switch(lvl)
	{
		case 0:
			Info(_log,"%s: %s", QSTRING_CSTR(_def.name), msg);
			break;
		case 1:
			Warning(_log,"%s: %s", QSTRING_CSTR(_def.name), msg);
			break;
		case 2:
			Error(_log,"%s: %s", QSTRING_CSTR(_def.name), msg);
			break;
		default:
			Debug(_log,"%s: %s", QSTRING_CSTR(_def.name), msg);
	}
}

const int Plugin::setComponentState(const int& comp, const int& enable)
{
	hyperion::Components comps;
	switch(comp)
	{
		case 0:
			_hyperion->getComponentRegister().setHyperionEnable(bool(enable));
			comps = hyperion::COMP_ALL;
			break;
		case 1:
			_hyperion->setComponentState(hyperion::COMP_SMOOTHING, bool(enable));
			comps = hyperion::COMP_SMOOTHING;
			break;
		case 2:
			_hyperion->setComponentState(hyperion::COMP_BLACKBORDER, bool(enable));
			comps = hyperion::COMP_BLACKBORDER;
			break;
		case 3:
			_hyperion->setComponentState(hyperion::COMP_LEDDEVICE, bool(enable));
			comps = hyperion::COMP_LEDDEVICE;
			break;
		case 4:
			_hyperion->setComponentState(hyperion::COMP_GRABBER, bool(enable));
			comps = hyperion::COMP_GRABBER;
			break;
		case 5:
			_hyperion->setComponentState(hyperion::COMP_V4L, bool(enable));
			comps = hyperion::COMP_V4L;
			break;
		default:
			Warning(_log, "%s: %s", QSTRING_CSTR(_def.name), "Requested an unknown component state change!");
			return 0;
	}
	Debug(_log, "%s: Set component '%s' state to %s", QSTRING_CSTR(_def.name), componentToString(comps), (enable ? "enabled" : "disabled"));
	return 1;
}

const QJsonValue Plugin::getSettings()
{
	return _plugins->getSettingsOfPlugin(_id);
}

void Plugin::setColor(const ColorRgb& color, const int& priority, const int& duration)
{
	_hyperion->setColor(priority, color, duration, "Plugin: "+_def.name);
}

int Plugin::setEffect(const char* name, const int& priority, const int& duration)
{
	return _hyperion->setEffect(name, priority, duration, "Plugin: "+_def.name);
}

void Plugin::handlePluginAction(PluginAction action, QString id, bool success, PluginDefinition def)
{
	// callback for saved actions
	if(action == P_SAVED && success && id == _id)
	{
		auto it = callbackObjects.find("ON_SETTINGS_CHANGED");
		if (it != callbackObjects.end())
		{
			PyObject *data = nullptr;
			// Verify that ON_SETTINGS_CHANGED is a proper callable
			if (it.value() && PyCallable_Check(it.value()))
			{
				// Call the callback function and return the result of the call on success, or NULL on failure.
				data = PyObject_CallObject(it.value(), NULL);
			}

			// handle exception
			if (!data && PyErr_Occurred())
				printException();

			// release "data" when done
			Py_XDECREF(data);
		}
	}
}

void Plugin::onCompStateChanged(const hyperion::Components comp, bool state)
{
	auto it = callbackObjects.find("ON_COMP_STATE_CHANGED");

	// Loop over all registered ON_COMP_STATE_CHANGED callbacks and verify that the callback is a proper callable
	while ( it != callbackObjects.end() && it.key() == "ON_COMP_STATE_CHANGED" && it.value() )
	{
		PyEval_RestoreThread(_state);
		PyThreadState * threadState = PyThreadState_New(_state->interp); // create a thread state object for this thread
		PyThreadState_Swap(threadState); // swap in the thread state

		PyObject *data = nullptr;

		if (PyList_Check(it.value())) // contains specific events
		{
			if (PyCallable_Check(PyList_GetItem(it.value(), 0)))
			{
				for (Py_ssize_t i = 1; i < PyList_Size(it.value()); i++) // loop over specific events
				{
					if (PyLong_Check(PyList_GetItem(it.value(), i))) // borrowed references
					{
						// the compared enum comp is added with 100 because the registered integer value has been added to not conflict with the callback enums
						if (PyLong_AsLong(PyList_GetItem(it.value(), i)) == (comp + 3))
						{
							// Call the callback function and return the result of the call on success, or NULL on failure.
							data = PyObject_CallFunctionObjArgs(PyList_GetItem(it.value(), 0), PyUnicode_FromString(componentToIdString(comp)), PyBool_FromLong(state), NULL);

							if (!data && PyErr_Occurred()) // handle exception
								printException();
							else
								Py_XDECREF(data); // release "data" when done
						}
					}
				}
			}
		}
		else // all events
		{
			if (PyCallable_Check(it.value()))
			{
				// call the callback function and return the result of the call on success, or NULL on failure.
				data = PyObject_CallFunctionObjArgs(it.value(), PyUnicode_FromString(componentToIdString(comp)), PyBool_FromLong(state), NULL);

				if (!data && PyErr_Occurred()) // handle exception
					printException();
				else
					Py_XDECREF(data); // release "data" when done
			}
		}

		PyThreadState_Swap(NULL); // swap the thread state out of the interpreter
		PyThreadState_Delete(threadState); // delete the thread state object
		PyEval_ReleaseLock(); // release the lock
		++it;
	}
}

void Plugin::onVisiblePriorityChanged(const quint8& priority)
{
	auto it = callbackObjects.find("ON_VISIBLE_PRIORITY_CHANGED");
	if ( it != callbackObjects.end() && it.key() == "ON_VISIBLE_PRIORITY_CHANGED")
	{
		PyEval_RestoreThread(_state);
		PyThreadState * threadState = PyThreadState_New(_state->interp); // create a thread state object for this thread
		PyThreadState_Swap(threadState); // swap in the thread state

		PyObject *data = nullptr;

		// Verify that ON_VISIBLE_PRIORITY_CHANGED is a proper callable
		if (it.value() && PyCallable_Check(it.value()))
		{
			// Call the callback function and return the result of the call on success, or NULL on failure.
			data = PyObject_CallFunctionObjArgs(it.value(), PyLong_FromUnsignedLong(priority), NULL);
		}

		// handle exception
		if (!data && PyErr_Occurred())
			printException();

		// release "data" when done
		Py_XDECREF(data);

		PyThreadState_Swap(NULL); // swap the thread state out of the interpreter
		PyThreadState_Delete(threadState); // delete the thread state object
		PyEval_ReleaseLock(); // release the lock
	}
}
