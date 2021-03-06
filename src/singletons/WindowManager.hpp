#pragma once

#include "common/Channel.hpp"
#include "common/FlagsEnum.hpp"
#include "common/Singleton.hpp"
#include "widgets/splits/SplitContainer.hpp"

namespace chatterino {

class Settings;
class Paths;
class Window;
class SplitContainer;

enum class MessageElementFlag;
using MessageElementFlags = FlagsEnum<MessageElementFlag>;
enum class WindowType;

enum class SettingsDialogPreference;

class WindowManager final : public Singleton
{
public:
    WindowManager();

    static void encodeChannel(IndirectChannel channel, QJsonObject &obj);
    static IndirectChannel decodeChannel(const QJsonObject &obj);

    static int clampUiScale(int scale);
    static float getUiScaleValue();
    static float getUiScaleValue(int scale);

    static const int uiScaleMin;
    static const int uiScaleMax;

    void showSettingsDialog(
        SettingsDialogPreference preference = SettingsDialogPreference());

    // Show the account selector widget at point
    void showAccountSelectPopup(QPoint point);

    // Tell a channel (or all channels if channel is nullptr) to redo their
    // layout
    void layoutChannelViews(Channel *channel = nullptr);

    // Force all channel views to redo their layout
    // This is called, for example, when the emote scale or timestamp format has
    // changed
    void forceLayoutChannelViews();
    void repaintVisibleChatWidgets(Channel *channel = nullptr);
    void repaintGifEmotes();

    Window &getMainWindow();
    Window &getSelectedWindow();
    Window &createWindow(WindowType type);

    int windowCount();
    Window *windowAt(int index);

    virtual void initialize(Settings &settings, Paths &paths) override;
    virtual void save() override;
    void closeAll();

    int getGeneration() const;
    void incGeneration();

    MessageElementFlags getWordFlags();
    void updateWordTypeMask();

    pajlada::Signals::NoArgSignal repaintGifs;

    // This signal fires whenever views rendering a channel, or all views if the
    // channel is a nullptr, need to redo their layout
    pajlada::Signals::Signal<Channel *> layout;

    pajlada::Signals::NoArgSignal wordFlagsChanged;

    // Sends an alert to the main window
    // It reads the `longAlert` setting to decide whether the alert will expire
    // or not
    void sendAlert();

    // Queue up a save in the next 10 seconds
    // If a save was already queued up, we reset the to happen in 10 seconds
    // again
    void queueSave();

private:
    void encodeNodeRecusively(SplitContainer::Node *node, QJsonObject &obj);

    bool initialized_ = false;

    std::atomic<int> generation_{0};

    std::vector<Window *> windows_;

    Window *mainWindow_{};
    Window *selectedWindow_{};

    MessageElementFlags wordFlags_{};
    pajlada::Settings::SettingListener wordFlagsListener_;

    QTimer *saveTimer;
};

}  // namespace chatterino
