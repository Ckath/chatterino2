#include "BaseWindow.hpp"

#include "Application.hpp"
#include "boost/algorithm/algorithm.hpp"
#include "debug/Log.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "singletons/WindowManager.hpp"
#include "util/PostToThread.hpp"
#include "util/WindowsHelper.hpp"
#include "widgets/Label.hpp"
#include "widgets/TooltipWidget.hpp"
#include "widgets/helper/EffectLabel.hpp"
#include "widgets/helper/Shortcut.hpp"

#include <QApplication>
#include <QDebug>
#include <QDesktopWidget>
#include <QIcon>
#include <functional>

#ifdef USEWINSDK
#    include <ObjIdl.h>
#    include <VersionHelpers.h>
#    include <Windows.h>
#    include <dwmapi.h>
#    include <gdiplus.h>
#    include <windowsx.h>

//#include <ShellScalingApi.h>
#    pragma comment(lib, "Dwmapi.lib")

#    include <QHBoxLayout>
#    include <QVBoxLayout>

#    define WM_DPICHANGED 0x02E0
#endif

#include "widgets/helper/TitlebarButton.hpp"

namespace chatterino {

BaseWindow::BaseWindow(QWidget *parent, Flags _flags)
    : BaseWidget(parent,
                 Qt::Window | ((_flags & TopMost) ? Qt::WindowStaysOnTopHint
                                                  : Qt::WindowFlags()))
    , enableCustomFrame_(_flags & EnableCustomFrame)
    , frameless_(_flags & Frameless)
    , flags_(_flags)
{
    if (this->frameless_)
    {
        this->enableCustomFrame_ = false;
        this->setWindowFlag(Qt::FramelessWindowHint);
    }

    this->init();

    getSettings()->uiScale.connect(
        [this]() { postToThread([this] { this->updateScale(); }); },
        this->connections_);

    this->updateScale();

    createWindowShortcut(this, "CTRL+0",
                         [] { getSettings()->uiScale.setValue(0); });

    //    QTimer::this->scaleChangedEvent(this->getScale());
}

float BaseWindow::getScale() const
{
    return this->getOverrideScale().value_or(this->scale_);
}

BaseWindow::Flags BaseWindow::getFlags()
{
    return this->flags_;
}

void BaseWindow::init()
{
    this->setWindowIcon(QIcon(":/images/icon.png"));

#ifdef USEWINSDK
    if (this->hasCustomWindowFrame())
    {
        // CUSTOM WINDOW FRAME
        QVBoxLayout *layout = new QVBoxLayout();
        this->ui_.windowLayout = layout;
        layout->setContentsMargins(0, 1, 0, 0);
        layout->setSpacing(0);
        this->setLayout(layout);
        {
            if (!this->frameless_)
            {
                QHBoxLayout *buttonLayout = this->ui_.titlebarBox =
                    new QHBoxLayout();
                buttonLayout->setMargin(0);
                layout->addLayout(buttonLayout);

                // title
                Label *title = new Label("Chatterino");
                QObject::connect(
                    this, &QWidget::windowTitleChanged,
                    [title](const QString &text) { title->setText(text); });

                QSizePolicy policy(QSizePolicy::Ignored,
                                   QSizePolicy::Preferred);
                policy.setHorizontalStretch(1);
                title->setSizePolicy(policy);
                buttonLayout->addWidget(title);
                this->ui_.titleLabel = title;

                // buttons
                TitleBarButton *_minButton = new TitleBarButton;
                _minButton->setButtonStyle(TitleBarButtonStyle::Minimize);
                TitleBarButton *_maxButton = new TitleBarButton;
                _maxButton->setButtonStyle(TitleBarButtonStyle::Maximize);
                TitleBarButton *_exitButton = new TitleBarButton;
                _exitButton->setButtonStyle(TitleBarButtonStyle::Close);

                QObject::connect(_minButton, &TitleBarButton::leftClicked, this,
                                 [this] {
                                     this->setWindowState(Qt::WindowMinimized |
                                                          this->windowState());
                                 });
                QObject::connect(_maxButton, &TitleBarButton::leftClicked, this,
                                 [this, _maxButton] {
                                     this->setWindowState(
                                         _maxButton->getButtonStyle() !=
                                                 TitleBarButtonStyle::Maximize
                                             ? Qt::WindowActive
                                             : Qt::WindowMaximized);
                                 });
                QObject::connect(_exitButton, &TitleBarButton::leftClicked,
                                 this, [this] { this->close(); });

                this->ui_.minButton = _minButton;
                this->ui_.maxButton = _maxButton;
                this->ui_.exitButton = _exitButton;

                this->ui_.buttons.push_back(_minButton);
                this->ui_.buttons.push_back(_maxButton);
                this->ui_.buttons.push_back(_exitButton);

                //            buttonLayout->addStretch(1);
                buttonLayout->addWidget(_minButton);
                buttonLayout->addWidget(_maxButton);
                buttonLayout->addWidget(_exitButton);
                buttonLayout->setSpacing(0);
            }
        }
        this->ui_.layoutBase = new BaseWidget(this);
        layout->addWidget(this->ui_.layoutBase);
    }

// DPI
//    auto dpi = getWindowDpi(this->winId());

//    if (dpi) {
//        this->scale = dpi.value() / 96.f;
//    }
#endif

#ifdef USEWINSDK
    // fourtf: don't ask me why we need to delay this
    if (!(this->flags_ & Flags::TopMost))
    {
        QTimer::singleShot(1, this, [this] {
            getSettings()->windowTopMost.connect(
                [this](bool topMost, auto) {
                    ::SetWindowPos(HWND(this->winId()),
                                   topMost ? HWND_TOPMOST : HWND_NOTOPMOST, 0,
                                   0, 0, 0,
                                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                },
                this->managedConnections_);
        });
    }
#else
//    if (getSettings()->windowTopMost.getValue()) {
//        this->setWindowFlag(Qt::WindowStaysOnTopHint);
//    }
#endif
}

void BaseWindow::setStayInScreenRect(bool value)
{
    this->stayInScreenRect_ = value;

    this->moveIntoDesktopRect(this);
}

bool BaseWindow::getStayInScreenRect() const
{
    return this->stayInScreenRect_;
}

void BaseWindow::setActionOnFocusLoss(ActionOnFocusLoss value)
{
    this->actionOnFocusLoss_ = value;
}

BaseWindow::ActionOnFocusLoss BaseWindow::getActionOnFocusLoss() const
{
    return this->actionOnFocusLoss_;
}

QWidget *BaseWindow::getLayoutContainer()
{
    if (this->hasCustomWindowFrame())
    {
        return this->ui_.layoutBase;
    }
    else
    {
        return this;
    }
}

bool BaseWindow::hasCustomWindowFrame()
{
#ifdef USEWINSDK
    static bool isWin8 = IsWindows8OrGreater();

    return isWin8 && this->enableCustomFrame_;
#else
    return false;
#endif
}

void BaseWindow::themeChangedEvent()
{
    if (this->hasCustomWindowFrame())
    {
        QPalette palette;
        palette.setColor(QPalette::Background, QColor(0, 0, 0, 0));
        palette.setColor(QPalette::Foreground, this->theme->window.text);
        this->setPalette(palette);

        if (this->ui_.titleLabel)
        {
            QPalette palette_title;
            palette_title.setColor(
                QPalette::Foreground,
                this->theme->isLightTheme() ? "#333" : "#ccc");
            this->ui_.titleLabel->setPalette(palette_title);
        }

        for (Button *button : this->ui_.buttons)
        {
            button->setMouseEffectColor(this->theme->window.text);
        }
    }
    else
    {
        QPalette palette;
        palette.setColor(QPalette::Background, this->theme->window.background);
        palette.setColor(QPalette::Foreground, this->theme->window.text);
        this->setPalette(palette);
    }
}

bool BaseWindow::event(QEvent *event)
{
    if (event->type() ==
        QEvent::WindowDeactivate /*|| event->type() == QEvent::FocusOut*/)
    {
        this->onFocusLost();
    }

    return QWidget::event(event);
}

void BaseWindow::wheelEvent(QWheelEvent *event)
{
    if (event->orientation() != Qt::Vertical)
    {
        return;
    }

    if (event->modifiers() & Qt::ControlModifier)
    {
        if (event->delta() > 0)
        {
            getSettings()->uiScale.setValue(WindowManager::clampUiScale(
                getSettings()->uiScale.getValue() + 1));
        }
        else
        {
            getSettings()->uiScale.setValue(WindowManager::clampUiScale(
                getSettings()->uiScale.getValue() - 1));
        }
    }
}

void BaseWindow::onFocusLost()
{
    switch (this->getActionOnFocusLoss())
    {
        case Delete:
        {
            this->deleteLater();
        }
        break;

        case Close:
        {
            this->close();
        }
        break;

        case Hide:
        {
            this->hide();
        }
        break;

        default:;
    }
}

void BaseWindow::mousePressEvent(QMouseEvent *event)
{
#ifndef Q_OS_WIN
    if (this->flags_ & FramelessDraggable)
    {
        this->movingRelativePos = event->localPos();
        if (auto widget =
                this->childAt(event->localPos().x(), event->localPos().y()))
        {
            std::function<bool(QWidget *)> recursiveCheckMouseTracking;
            recursiveCheckMouseTracking = [&](QWidget *widget) {
                if (widget == nullptr)
                {
                    return false;
                }

                if (widget->hasMouseTracking())
                {
                    return true;
                }

                return recursiveCheckMouseTracking(widget->parentWidget());
            };

            if (!recursiveCheckMouseTracking(widget))
            {
                log("Start moving");
                this->moving = true;
            }
        }
    }
#endif

    BaseWidget::mousePressEvent(event);
}

void BaseWindow::mouseReleaseEvent(QMouseEvent *event)
{
#ifndef Q_OS_WIN
    if (this->flags_ & FramelessDraggable)
    {
        if (this->moving)
        {
            log("Stop moving");
            this->moving = false;
        }
    }
#endif

    BaseWidget::mouseReleaseEvent(event);
}

void BaseWindow::mouseMoveEvent(QMouseEvent *event)
{
#ifndef Q_OS_WIN
    if (this->flags_ & FramelessDraggable)
    {
        if (this->moving)
        {
            const auto &newPos = event->screenPos() - this->movingRelativePos;
            this->move(newPos.x(), newPos.y());
        }
    }
#endif

    BaseWidget::mouseMoveEvent(event);
}

TitleBarButton *BaseWindow::addTitleBarButton(const TitleBarButtonStyle &style,
                                              std::function<void()> onClicked)
{
    TitleBarButton *button = new TitleBarButton;
    button->setScaleIndependantSize(30, 30);

    this->ui_.buttons.push_back(button);
    this->ui_.titlebarBox->insertWidget(1, button);
    button->setButtonStyle(style);

    QObject::connect(button, &TitleBarButton::leftClicked, this,
                     [onClicked] { onClicked(); });

    return button;
}

EffectLabel *BaseWindow::addTitleBarLabel(std::function<void()> onClicked)
{
    EffectLabel *button = new EffectLabel;
    button->setScaleIndependantHeight(30);

    this->ui_.buttons.push_back(button);
    this->ui_.titlebarBox->insertWidget(1, button);

    QObject::connect(button, &EffectLabel::leftClicked, this,
                     [onClicked] { onClicked(); });

    return button;
}

void BaseWindow::changeEvent(QEvent *)
{
    TooltipWidget::getInstance()->hide();

#ifdef USEWINSDK
    if (this->ui_.maxButton)
    {
        this->ui_.maxButton->setButtonStyle(
            this->windowState() & Qt::WindowMaximized
                ? TitleBarButtonStyle::Unmaximize
                : TitleBarButtonStyle::Maximize);
    }
#endif

#ifndef Q_OS_WIN
    this->update();
#endif
}

void BaseWindow::leaveEvent(QEvent *)
{
    TooltipWidget::getInstance()->hide();
}

void BaseWindow::moveTo(QWidget *parent, QPoint point, bool offset)
{
    if (offset)
    {
        point.rx() += 16;
        point.ry() += 16;
    }

    this->move(point);
    this->moveIntoDesktopRect(parent);
}

void BaseWindow::resizeEvent(QResizeEvent *)
{
    // Queue up save because: Window resized
    getApp()->windows->queueSave();

    this->moveIntoDesktopRect(this);

    this->calcButtonsSizes();
}

void BaseWindow::moveEvent(QMoveEvent *event)
{
    // Queue up save because: Window position changed
    getApp()->windows->queueSave();

    BaseWidget::moveEvent(event);
}

void BaseWindow::closeEvent(QCloseEvent *)
{
    this->closing.invoke();
}

void BaseWindow::moveIntoDesktopRect(QWidget *parent)
{
    if (!this->stayInScreenRect_)
        return;

    // move the widget into the screen geometry if it's not already in there
    QDesktopWidget *desktop = QApplication::desktop();

    QRect s = desktop->availableGeometry(parent);
    QPoint p = this->pos();

    if (p.x() < s.left())
    {
        p.setX(s.left());
    }
    if (p.y() < s.top())
    {
        p.setY(s.top());
    }
    if (p.x() + this->width() > s.right())
    {
        p.setX(s.right() - this->width());
    }
    if (p.y() + this->height() > s.bottom())
    {
        p.setY(s.bottom() - this->height());
    }

    if (p != this->pos())
        this->move(p);
}

bool BaseWindow::nativeEvent(const QByteArray &eventType, void *message,
                             long *result)
{
#ifdef USEWINSDK
#    if (QT_VERSION == QT_VERSION_CHECK(5, 11, 1))
    MSG *msg = *reinterpret_cast<MSG **>(message);
#    else
    MSG *msg = reinterpret_cast<MSG *>(message);
#    endif

    bool returnValue = false;

    switch (msg->message)
    {
        case WM_DPICHANGED:
            returnValue = handleDPICHANGED(msg);
            break;

        case WM_SHOWWINDOW:
            returnValue = this->handleSHOWWINDOW(msg);
            break;

        case WM_NCCALCSIZE:
            returnValue = this->handleNCCALCSIZE(msg, result);
            break;

        case WM_SIZE:
            returnValue = this->handleSIZE(msg);
            break;

        case WM_NCHITTEST:
            returnValue = this->handleNCHITTEST(msg, result);
            break;

        default:
            return QWidget::nativeEvent(eventType, message, result);
    }

    QWidget::nativeEvent(eventType, message, result);

    return returnValue;
#else
    return QWidget::nativeEvent(eventType, message, result);
#endif
}

void BaseWindow::scaleChangedEvent(float)
{
#ifdef USEWINSDK
    this->calcButtonsSizes();
#endif
}

void BaseWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    if (this->frameless_)
    {
        painter.setPen(QColor("#999"));
        painter.drawRect(0, 0, this->width() - 1, this->height() - 1);
    }

    this->drawCustomWindowFrame(painter);
}

void BaseWindow::updateScale()
{
    auto scale =
        this->nativeScale_ * (this->flags_ & DisableCustomScaling
                                  ? 1
                                  : getApp()->windows->getUiScaleValue());
    this->setScale(scale);

    for (auto child : this->findChildren<BaseWidget *>())
    {
        child->setScale(scale);
    }
}

void BaseWindow::calcButtonsSizes()
{
    if (!this->shown_)
    {
        return;
    }

    if ((this->width() / this->getScale()) < 300)
    {
        if (this->ui_.minButton)
            this->ui_.minButton->setScaleIndependantSize(30, 30);
        if (this->ui_.maxButton)
            this->ui_.maxButton->setScaleIndependantSize(30, 30);
        if (this->ui_.exitButton)
            this->ui_.exitButton->setScaleIndependantSize(30, 30);
    }
    else
    {
        if (this->ui_.minButton)
            this->ui_.minButton->setScaleIndependantSize(46, 30);
        if (this->ui_.maxButton)
            this->ui_.maxButton->setScaleIndependantSize(46, 30);
        if (this->ui_.exitButton)
            this->ui_.exitButton->setScaleIndependantSize(46, 30);
    }
}

void BaseWindow::drawCustomWindowFrame(QPainter &painter)
{
#ifdef USEWINSDK
    if (this->hasCustomWindowFrame())
    {
        QPainter painter(this);

        QColor bg = this->overrideBackgroundColor_.value_or(
            this->theme->window.background);

        painter.fillRect(QRect(0, 1, this->width() - 0, this->height() - 0),
                         bg);
    }
#endif
}

bool BaseWindow::handleDPICHANGED(MSG *msg)
{
#ifdef USEWINSDK
    int dpi = HIWORD(msg->wParam);

    float _scale = dpi / 96.f;

    static bool firstResize = true;

    if (!firstResize)
    {
        auto *prcNewWindow = reinterpret_cast<RECT *>(msg->lParam);
        SetWindowPos(msg->hwnd, nullptr, prcNewWindow->left, prcNewWindow->top,
                     prcNewWindow->right - prcNewWindow->left,
                     prcNewWindow->bottom - prcNewWindow->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
    firstResize = false;

    this->nativeScale_ = _scale;
    this->updateScale();

    return true;
#else
    return false;
#endif
}

bool BaseWindow::handleSHOWWINDOW(MSG *msg)
{
#ifdef USEWINSDK
    if (auto dpi = getWindowDpi(msg->hwnd))
    {
        this->nativeScale_ = dpi.get() / 96.f;
        this->updateScale();
    }

    if (!this->shown_ && this->isVisible() && this->hasCustomWindowFrame())
    {
        this->shown_ = true;

        const MARGINS shadow = {8, 8, 8, 8};
        DwmExtendFrameIntoClientArea(HWND(this->winId()), &shadow);
    }

    this->calcButtonsSizes();

    return true;
#else
    return false;
#endif
}

bool BaseWindow::handleNCCALCSIZE(MSG *msg, long *result)
{
#ifdef USEWINSDK
    if (this->hasCustomWindowFrame())
    {
        // int cx = GetSystemMetrics(SM_CXSIZEFRAME);
        // int cy = GetSystemMetrics(SM_CYSIZEFRAME);

        if (msg->wParam == TRUE)
        {
            NCCALCSIZE_PARAMS *ncp =
                (reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam));
            ncp->lppos->flags |= SWP_NOREDRAW;
            RECT *clientRect = &ncp->rgrc[0];

            clientRect->left += 1;
            clientRect->top += 0;
            clientRect->right -= 1;
            clientRect->bottom -= 1;
        }

        *result = 0;
        return true;
    }
    return false;
#else
    return false;
#endif
}

bool BaseWindow::handleSIZE(MSG *msg)
{
#ifdef USEWINSDK
    if (this->ui_.windowLayout)
    {
        if (this->frameless_)
        {
            //
        }
        else if (this->hasCustomWindowFrame())
        {
            if (msg->wParam == SIZE_MAXIMIZED)
            {
                auto offset = int(this->getScale() * 8);

                this->ui_.windowLayout->setContentsMargins(offset, offset,
                                                           offset, offset);
            }
            else
            {
                this->ui_.windowLayout->setContentsMargins(0, 1, 0, 0);
            }
        }
    }
    return false;
#else
    return false;
#endif
}

bool BaseWindow::handleNCHITTEST(MSG *msg, long *result)
{
#ifdef USEWINSDK
    const LONG border_width = 8;  // in pixels
    RECT winrect;
    GetWindowRect(HWND(winId()), &winrect);

    long x = GET_X_LPARAM(msg->lParam);
    long y = GET_Y_LPARAM(msg->lParam);

    QPoint point(x - winrect.left, y - winrect.top);

    if (this->hasCustomWindowFrame())
    {
        *result = 0;

        bool resizeWidth = minimumWidth() != maximumWidth();
        bool resizeHeight = minimumHeight() != maximumHeight();

        if (resizeWidth)
        {
            // left border
            if (x < winrect.left + border_width)
            {
                *result = HTLEFT;
            }
            // right border
            if (x >= winrect.right - border_width)
            {
                *result = HTRIGHT;
            }
        }
        if (resizeHeight)
        {
            // bottom border
            if (y >= winrect.bottom - border_width)
            {
                *result = HTBOTTOM;
            }
            // top border
            if (y < winrect.top + border_width)
            {
                *result = HTTOP;
            }
        }
        if (resizeWidth && resizeHeight)
        {
            // bottom left corner
            if (x >= winrect.left && x < winrect.left + border_width &&
                y < winrect.bottom && y >= winrect.bottom - border_width)
            {
                *result = HTBOTTOMLEFT;
            }
            // bottom right corner
            if (x < winrect.right && x >= winrect.right - border_width &&
                y < winrect.bottom && y >= winrect.bottom - border_width)
            {
                *result = HTBOTTOMRIGHT;
            }
            // top left corner
            if (x >= winrect.left && x < winrect.left + border_width &&
                y >= winrect.top && y < winrect.top + border_width)
            {
                *result = HTTOPLEFT;
            }
            // top right corner
            if (x < winrect.right && x >= winrect.right - border_width &&
                y >= winrect.top && y < winrect.top + border_width)
            {
                *result = HTTOPRIGHT;
            }
        }

        if (*result == 0)
        {
            bool client = false;

            for (QWidget *widget : this->ui_.buttons)
            {
                if (widget->geometry().contains(point))
                {
                    client = true;
                }
            }

            if (this->ui_.layoutBase->geometry().contains(point))
            {
                client = true;
            }

            if (client)
            {
                *result = HTCLIENT;
            }
            else
            {
                *result = HTCAPTION;
            }
        }

        return true;
    }
    else if (this->flags_ & FramelessDraggable)
    {
        *result = 0;
        bool client = false;

        if (auto widget = this->childAt(point))
        {
            std::function<bool(QWidget *)> recursiveCheckMouseTracking;
            recursiveCheckMouseTracking = [&](QWidget *widget) {
                if (widget == nullptr)
                {
                    return false;
                }

                if (widget->hasMouseTracking())
                {
                    return true;
                }

                return recursiveCheckMouseTracking(widget->parentWidget());
            };

            if (recursiveCheckMouseTracking(widget))
            {
                client = true;
            }
        }

        if (client)
        {
            *result = HTCLIENT;
        }
        else
        {
            *result = HTCAPTION;
        }

        return true;
    }
#else
    return false;
#endif
}

}  // namespace chatterino
