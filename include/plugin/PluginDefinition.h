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

typedef struct PyEnumDef
{
	char* name;
	int value;
} PyEnumDef;

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

enum CallbackAction
{
	ON_COMP_STATE_CHANGED,
	ON_SETTINGS_CHANGED,
	ON_VISIBLE_PRIORITY_CHANGED
};
