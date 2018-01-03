#pragma once

#include <QMap>
#include <QJsonArray>
#include <QJsonObject>

struct PluginDefinition
{
	QString         name;
	QString         description;
	QString         version;
	QJsonObject     dependencies;
	QJsonArray      changelog;
	QString         provider;
	QString         support;
	QString         source;
	QString         entryPy;
	QJsonObject     settingsSchema;
	QJsonObject     settings;
};

enum PluginAction
{
	P_INSTALL,
	P_INSTALLED,
	P_REMOVE,
	P_REMOVED,
	P_START,
	P_STARTED,
	P_STOP,
	P_STOPPED,
	P_SAVE,
	P_SAVED,
	P_AUTOUPDATE,
	P_AUTOUPDATED,
	P_ERROR,
	P_UPD_AVAIL,
	P_UPDATED_AVAIL
};

// type definition for callback enums
 typedef enum
 {
	ON_COMP_STATE_CHANGED,
	ON_SETTINGS_CHANGED,
	CALLBACK_INVALID
 } callback_type_t;

inline const char* callbackTypeToString(int callback)
{
	switch (callback) 
	{
		case ON_COMP_STATE_CHANGED:				return "ON_COMP_STATE_CHANGED";
		case ON_SETTINGS_CHANGED:					return "ON_SETTINGS_CHANGED";
		default:														return "";
	}
}

inline callback_type_t stringToCallbackType(QString string)
{
	string = string.toUpper();
	if (string == "ON_COMP_STATE_CHANGED")	return ON_COMP_STATE_CHANGED;
	if (string == "ON_SETTINGS_CHANGED")		return ON_SETTINGS_CHANGED;

	return CALLBACK_INVALID;
}
