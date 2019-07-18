#pragma once

// util
#include <utils/ColorRgb.h>
#include <utils/Components.h>

// qt
#include <QObject>

///
/// @brief Singelton API event sharing across Hyperion instances
///
class EventSharing : public QObject
{
	Q_OBJECT
public:
    static EventSharing* getInstance()
    {
        static EventSharing    instance;
        return & instance;
    }
private:
    EventSharing()
	{
		//qRegisterMetaType<>()
	}

public:
    EventSharing(EventSharing const&)          = delete;
    void operator=(EventSharing const&)  = delete;

signals:
	///
	/// @brief Set an effect
	/// @param hyperion      The "sender" Hyperion object
	/// @param effectName    The name of the effect
	/// @param priority      The priority
	/// @param timeout       The timeout
	/// @param origin        The origin
	///
	void requestSetEffect(QObject* hyperion, const QString &effectName, int priority, int timeout, const QString& origin);

	///
	/// @brief Set a color
	/// @param hyperion      The "sender" Hyperion object
	/// @param priority      The priority
	/// @param color         The single color
	/// @param timeout_ms    The timeout_ms
	/// @param origin        The origin
	///
	void requestSetColor(QObject* hyperion, const int priority, const ColorRgb &color, const int timeout_ms, const QString& origin);

	///
	/// @brief Set a color
	/// @param hyperion      The "sender" Hyperion object
	/// @param comp          The component
	/// @param state         The new state
	///
	void requestSetCompState(QObject* hyperion, const hyperion::Components component, const bool state);
};
