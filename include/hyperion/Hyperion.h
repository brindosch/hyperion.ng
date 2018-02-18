#pragma once

// stl includes
#include <list>
#include <QMap>

// QT includes
#include <QString>
#include <QStringList>
#include <QSize>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>

// hyperion-utils includes
#include <utils/Image.h>
#include <utils/ColorRgb.h>
#include <utils/Logger.h>
#include <utils/Components.h>
#include <utils/VideoMode.h>

// Hyperion includes
#include <hyperion/LedString.h>
#include <hyperion/PriorityMuxer.h>
#include <hyperion/ColorAdjustment.h>
#include <hyperion/ComponentRegister.h>

// Effect engine includes
#include <effectengine/EffectDefinition.h>
#include <effectengine/ActiveEffectDefinition.h>
#include <effectengine/EffectSchema.h>

// settings utils
#include <utils/settings.h>

class HyperionDaemon;
class ImageProcessor;
class LinearColorSmoothing;
class EffectEngine;
class MultiColorAdjustment;
class ColorAdjustment;
class Plugins;
class SettingsManager;
class BGEffectHandler;
class CaptureCont;
class BoblightServer;
class LedDeviceWrapper;

///
/// The main class of Hyperion. This gives other 'users' access to the attached LedDevice through
/// the priority muxer.
///
class Hyperion : public QObject
{
	Q_OBJECT
public:
	///
	/// Type definition of the info structure used by the priority muxer
	///
	typedef PriorityMuxer::InputInfo InputInfo;
	///
	/// RGB-Color channel enumeration
	///
	enum RgbChannel
	{
		BLACK, WHITE, RED, GREEN, BLUE, CYAN, MAGENTA, YELLOW, INVALID
	};

	///
	/// @brief Destructor; cleans up resources
	///
	virtual ~Hyperion();

	///
	/// @brief free all alocated objects, should be called only from constructor or before restarting hyperion
	/// param[in] emitCloseSignal Used by constructor
	///
	void freeObjects(bool emitCloseSignal=false);

	///
	/// @brief Get a pointer to the effect engine
	/// @return EffectEngine instance pointer
	///
	EffectEngine* getEffectEngineInstance() { return _effectEngine; };

	///
	/// @brief Get a pointer to the plugins instance
	/// @return Plugins instance pointer
	///
	Plugins* getPluginsInstance() { return _plugins; };

	///
	/// @brief Get a pointer to the priorityMuxer instance
	/// @return PriorityMuxer instance pointer
	///
	PriorityMuxer* getMuxerInstance() { return &_muxer; };

	ImageProcessor* getImageProcessor() { return _imageProcessor; };

	///
	/// @brief Get a setting by settings::type from SettingsManager
	/// @param[in] type The settingsType from enum
	/// @return Data Document
	///
	QJsonDocument getSetting(const settings::type& type);

	///
	/// @brief Get instance index of this instance
	/// @return The index of this instance
	///
	const quint8 & getInstanceIndex() { return _instIndex; };

	///
	/// @brief Save a complete json config
	/// @param[in] config The entire config object
	/// @param[in] correct If true will correct json against schema before save
	/// @return True on success else false
	///
	bool saveSettings(QJsonObject config, const bool& correct = false);

	///
	/// @brief Returns the number of attached leds
	///
	unsigned getLedCount() const;

	///
	/// @brief Return the size of led grid
	///
	QSize getLedGridSize() const { return _ledGridSize; };

	///
	/// @brief Returns the current priority
	/// @return The current priority
	///
	int getCurrentPriority() const;

	///
	/// @brief Returns true if current priority is given priority
	/// @return bool
	///
	bool isCurrentPriority(const int priority) const;

	///
	/// @brief Returns a list of all registered  priorities
	/// @return The list with priorities
	///
	QList<int> getActivePriorities() const;

	///
	/// Returns the information of a specific priorrity channel
	/// @param[in] priority  The priority channel
	/// @return The information of the given, a not found priority will return lowest priority as fallback
	///
	const InputInfo getPriorityInfo(const int priority) const;

	///
	/// @brief Save an effect
	/// @param[in] obj The effect args
	/// @param[out] resultMsg The feedback message
	/// @return True on success else false
	///
	const bool saveEffect(const QJsonObject& obj, QString& resultMsg);

	///
	/// @brief Delete an effect by name.
	/// @param[in] effectName The effect name to delete
	/// @param[out] resultMsg The message on error
	/// @return True on success else false
	///
	const bool deleteEffect(const QString& effectName, QString& resultMsg);

	///
	/// @brief Get the list of available effects
	/// @return The list of available effects
	///
	const std::list<EffectDefinition> &getEffects() const;

	///
	/// @brief Get the list of active effects
	/// @return The list of active effects
	///
	const std::list<ActiveEffectDefinition> &getActiveEffects();

	///
	/// Get the list of available effect schema files
	/// @return The list of available effect schema files
	///
	const std::list<EffectSchema> &getEffectSchemas();

	///
	/// @brief Gets the current json config object from SettingsManager
	/// @return The current json config
	///
	const QJsonObject & getQJsonConfig();

	///
	/// @brief Register a new input by priority, the priority is not active (timeout -100 isn't muxer recognized) until you start to update the data with setInput()
	/// A repeated call to update the base data of a known priority won't overwrite their current timeout
	/// @param[in] priority    The priority of the channel
	/// @param[in] component   The component of the channel
	/// @param[in] origin      Who set the channel (CustomString@IP)
	/// @param[in] owner       Speicifc owner string, might be empty
	/// @param[in] smooth_cfg  The smooth id to use
	///
	void registerInput(const int priority, const hyperion::Components& component, const QString& origin = "System", const QString& owner = "", unsigned smooth_cfg = 0);

	///
	/// @brief Enable/Disable automatic/priorized source selection
	/// @param[in] enabled The state
	///
	void setSourceAutoSelectEnabled(bool enabled);

	///
	/// @brief Set current input source to visible
	/// @param[in] priority The priority channel which should be vidible
	/// @return True if success, false on error
	///
	bool setCurrentSourcePriority(int priority );

	///
	/// @brief Gets current state of automatic/priorized source selection
	/// @return The current state
	///
	bool sourceAutoSelectEnabled();

	///
	/// @brief Called from components to update their current state. DO NOT CALL FROM USERS
	/// @param[in] component The component from enum
	/// @param[in] state The state of the component [true | false]
	///
	void setNewComponentState(const hyperion::Components& component, const bool& state);

	///
	/// @brief Enable/Disable components during runtime, called from external API (requests)
	/// @param[in] component The component from enum
	/// @param[in] state The state of the component [true | false]
	///
	void setComponentState(const hyperion::Components component, const bool state);

	///
	/// @brief Get current state of a component
	/// @param[in] component The component from enum
	/// @return 1 if enabled, 0 if disabled, -1 if not found;
	///
	int getComponentState(const hyperion::Components& component) const;

	///
	/// @brief Gets the register that holds component states
	/// @return The component register
	///
	ComponentRegister& getComponentRegister() { return _componentRegister; };

	///
	/// @brief Gets the methode how image is maped to leds
	/// @return Returns the current mapping type
	///
	const int & getLedMappingType();

	///
	/// @brief Get the root path for all hyperion user data files
	/// @return Root path for all hyperion user data files
	///
	const QString &getRootPath() { return _rootPath; };

	///
	/// @brief Get the current latchtime of the ledDevice
	/// @return latchtime in ms
	///
	int getLatchTime() const;

	///
	/// @brief forward smoothing config
	/// 
	unsigned addSmoothingConfig(int settlingTime_ms, double ledUpdateFrequency_hz=25.0, unsigned updateDelay=0);

	const VideoMode & getCurrentVideoMode();

	///
	/// @brief Get the current active led device
	/// @return The device name
	///
	const QString & getActiveDevice();

public slots:
	///
	/// @brief Update the current color of a priority (prev registered with registerInput())
	/// DO NOT use this together with setInputImage() at the same time!
	/// @param[in]  priority     The priority to update
	/// @param[in]  ledColors    The colors
	/// @param[in]  timeout_ms   The new timeout (defaults to -1 endless)
	/// @param[in]  clearEffect  Should be true when NOT called from an effect
	/// @return              True on success, false when priority is not found
	///
	const bool setInput(const int priority, const std::vector<ColorRgb>& ledColors, const int timeout_ms = -1, const bool& clearEffect = true);

	///
	/// @brief Update the current image of a priority (prev registered with registerInput())
	/// DO NOT use this together with setInput() at the same time!
	/// @param[in]  priority     The priority to update
	/// @param[in]  image        The new image
	/// @param[in]  timeout_ms   The new timeout (defaults to -1 endless)
	/// @param[in]  clearEffect  Should be true when NOT called from an effect
	/// @return              True on success, false when priority is not found
	///
	const bool setInputImage(const int priority, const Image<ColorRgb>& image, int64_t timeout_ms = -1, const bool& clearEffect = true);

	///
	/// @brief Set the given priority to inactive
	/// @param[in] priority The priority
	/// @return True on success false if not found
	///
	const bool setInputInactive(const quint8& priority);

	///
	/// @brief Writes a single color to all the leds for the given time and priority
	/// Registers comp color or provided type against muxer
	/// Should be never used to update leds continuous
	///
	/// @param[in] priority The priority of the written color
	/// @param[in] ledColor The color to write to the leds
	/// @param[in] origin   The setter
	/// @param[in] timeout_ms The time the leds are set to the given color [ms]
	///
	void setColor(int priority, const ColorRgb &ledColor, const int timeout_ms = -1, const QString& origin = "System" ,bool clearEffects = true);

	///
	/// @brief Returns the list with unique adjustment identifiers
	/// @return The list with adjustment identifiers
	///
	const QStringList & getAdjustmentIds() const;

	///
	/// @brief Returns the ColorAdjustment with the given identifier
	/// @return The adjustment with the given identifier (or nullptr if the identifier does not exist)
	///
	ColorAdjustment * getAdjustment(const QString& id);

	///
	/// @brief Tell Hyperion that the corrections have changed and the leds need to be updated
	///
	void adjustmentsUpdated();

	///
	/// @brief Clears the given priority channel. This will switch the led-colors to the colors of the next
	/// lower priority channel (or off if no more channels are set)
	/// @param[in] priority  The priority channel
	/// @return              True on success else false (not found)
	///
	const bool clear(int priority);

	///
	/// @brief Clears all priority channels. This will switch the leds off until a new priority is written.
	///
	void clearall(bool forceClearAll=false);

	/// @brief Run the specified effect on the given priority channel and optionally specify a timeout
	/// @param[in] effectName Name of the effec to run
	/// @param[in] priority The priority channel of the effect
	/// @param[in] timeout The timeout of the effect (after the timout, the effect will be cleared)
	/// @return True on success else false
	int setEffect(const QString & effectName, int priority, int timeout = -1, const QString & origin="System");

	/// @brief Run the specified effect on the given priority channel and optionally specify a timeout
	/// @param[in] effectName Name of the effec to run
	/// @param[in] args Arguments of the effect script
	/// @param[in] priority The priority channel of the effect
	/// @param[in] timeout The timeout of the effect (after the timout, the effect will be cleared)
	/// @return True on success else false
	int setEffect(const QString & effectName, const QJsonObject & args, int priority,
				  int timeout = -1, const QString & pythonScript = "", const QString & origin="System");

	///
	/// @brief Sets the methode how image is maped to leds at ImageProcessor
	/// @param[in] mappingType The mapping type
	void setLedMappingType(const int& mappingType);

	///
	/// @brief Set the video mode (2D/3D)
	/// @param[in] mode The new video mode
	///
	void setVideoMode(const VideoMode& mode);

	///
	/// @brief Init after thread start
	///
	void start();

	///
	/// @brief Stop the execution of this thread, helper to properly track eventing
	///
	void stop();

signals:
	///
	/// @brief Signal which is emitted when a priority channel is actively cleared.
	/// This signal will not be emitted when a priority channel time out
	///
	void channelCleared(int priority);

	///
	/// @brief Signal which is emitted when all priority channels are actively cleared.
	/// This signal will not be emitted when a priority channel time out
	///
	void allChannelsCleared();

	///
	/// @brief Emits whenever a user request a component state change, it's up the component to listen
	/// and update the component state at the componentRegister
	/// @param[in] component  The component from enum
	/// @param[in] enabled    The new state of the component
	///
	void componentStateChanged(const hyperion::Components component, bool enabled);

	///
	/// @brief Emits whenever the imageToLedsMapping has changed
	/// @param[in] mappingType The new mapping type
	///
	void imageToLedsMappingChanged(const int& mappingType);

	///
	/// @brief Emits whenever the visible priority delivers a image which is applied in update()
	/// priorities with ledColors won't emit this signal
	/// @param[in] image The current image
	///
	void currentImage(const Image<ColorRgb> & image);

	void closing();

	///
	/// @brief Signal which is emitted, when a new json message should be forwarded
	///
	void forwardJsonMessage(QJsonObject);

	///
	/// @brief Is emitted from clients who request a videoMode change
	///
	void videoMode(const VideoMode& mode);

	///
	/// @brief A new videoMode has been set (called from HyperionIManager!)
	///
	void newVideoMode(const VideoMode& mode);

	///
	/// @brief Emits whenever a config part changed. SIGNAL PIPE helper for SettingsManager -> HyperionDaemon
	/// @param[in] type The settings type from enum
	/// @param[in] data The data as QJsonDocument
	///
	void settingsChanged(const settings::type& type, const QJsonDocument& data);

	///
	/// @brief Emits whenever the adjustments have been updated
	///
	void adjustmentChanged();

	///
	/// @brief Signal pipe from EffectEngine to external, emits when effect list has been updated
	///
	void effectListUpdated();

	///
	/// @brief Emits whenever new data should be pushed to the LedDeviceWrapper which forwards it to the threaded LedDevice
	/// @param[in] ledValues The led values
	///
	void ledDeviceData(const std::vector<ColorRgb>& ledValues);

	///
	/// @brief Emits whenever new untransformed ledColos data is available, reflects the current visible device
	/// @param[in] ledValues The led values
	///
	void rawLedColors(const std::vector<ColorRgb>& ledValues);


	///
	/// @brief Emits before thread quit is requested
	///
	void finished();

	///
	/// @brief Emits after thread has been started
	///
	void started();

private slots:
	///
	/// @briefUpdates the priority muxer with the current time and (re)writes the led color with applied transforms.
	///
	void update();

	///
	///	@brief Apply ComponentRegister emits for COMP_ALL. Enables/Disables core timers
	///	@param[in] comp The component
	///	@param[in] state The new state of the component
	///
	void updatedComponentState(const hyperion::Components comp, const bool state);

	///
	///	@brief Apply settings updates for LEDS and COLOR
	///	@param[in] type The type from enum
	///	@param[in] config The configuration
	///
	void handleSettingsUpdate(const settings::type& type, const QJsonDocument& config);

	///
	/// @brief Apply new videoMode from Daemon to _currVideoMode
	///
	void handleNewVideoMode(const VideoMode& mode) { _currVideoMode = mode; };

private:
	friend class HyperionDaemon;
	friend class HyperionIManager;
	///
	/// @brief Constructs the Hyperion instance, just accessible for HyperionIManager
	/// @param  instance  The instance index
	/// @param  rootPath  The rootPath of user data
	///
	Hyperion(const quint8& instance, const QString& rootPath);

	/// instance index
	const quint8 _instIndex;

	/// Settings manager of this instance
	SettingsManager* _settingsManager;

	/// Register that holds component states
	ComponentRegister _componentRegister;

	/// The specifiation of the led frame construction and picture integration
	LedString _ledString;

	/// specifiation of cloned leds
	LedString _ledStringClone;

	/// Image Processor
	ImageProcessor* _imageProcessor;

	std::vector<ColorOrder> _ledStringColorOrder;

	/// The priority muxer
	PriorityMuxer _muxer;

	/// The adjustment from raw colors to led colors
	MultiColorAdjustment * _raw2ledAdjustment;

	/// The actual LedDeviceWrapper
	LedDeviceWrapper* _ledDeviceWrapper;

	/// The smoothing LedDevice
	LinearColorSmoothing * _deviceSmooth;

	/// Effect engine
	EffectEngine * _effectEngine;

	/// plugins instance
	Plugins * _plugins;

	/// root path for all hyperion user data files
	QString _rootPath;

	/// Logger instance
	Logger * _log;

	/// count of hardware leds
	unsigned _hwLedCount;

	QSize _ledGridSize;

	/// Store the previous compID for smarter update()
	hyperion::Components   _prevCompId;

	/// Background effect instance, kept active to react on setting changes
	BGEffectHandler* _BGEffectHandler;
	/// Capture control for Daemon native capture
	CaptureCont* _captureCont;

	// lock Hyperion::update() for exec
	bool _lockUpdate = false;

	/// buffer for leds (with adjustment)
	std::vector<ColorRgb> _ledBuffer;

	VideoMode _currVideoMode = VIDEO_2D;

	/// Boblight instance
	BoblightServer* _boblightServer;
};
