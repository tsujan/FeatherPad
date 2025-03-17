/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2025 <tsujan2000@gmail.com>
 *
 * FeatherPad is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FeatherPad is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @license GPL-3.0+ <https://spdx.org/licenses/GPL-3.0+.html>
 */

#ifndef WARNINGBAR_H
#define WARNINGBAR_H

#include <QPointer>
#include <QEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QGridLayout>
#include <QPalette>
#include <QLabel>
#include <QPropertyAnimation>

#define DURATION 150

namespace FeatherPad {

class WarningBar : public QWidget
{
    Q_OBJECT
public:
    WarningBar (const QString& message, const int verticalOffset = 0, int timeout = 10, QWidget *parent = nullptr)
        : QWidget (parent), timer_ (nullptr) {
        int anotherBar (false);
        if (parent)
        { // show only one warning bar at a time
            const QList<WarningBar*> warningBars = parent->findChildren<WarningBar*>();
            for (WarningBar *wb : warningBars)
            {
                if (wb != this)
                {
                    wb->closeBar();
                    anotherBar = true;
                }
            }
        }

        message_ = message;
        vOffset_ = verticalOffset;
        isClosing_ = false;
        mousePressed_ = false;

        /* make it like a translucent layer */
        setAutoFillBackground (true);
        QPalette p = palette();
        p.setColor (foregroundRole(), Qt::white);
        p.setColor (backgroundRole(), timeout > 0 ? QColor (125, 0, 0, 200) : QColor (0, 70, 0, 210));
        setPalette (p);

        grid_ = new QGridLayout;
        grid_->setContentsMargins (5, 0, 5 ,5); // the top margin is added when setting the geometry
        /* use a spacer to compress the label vertically */
        QSpacerItem *spacer = new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
        grid_->addItem (spacer, 0, 0);
        /* add the label */
        QLabel *warningLabel = new QLabel (message);
        warningLabel->setAttribute (Qt::WA_TransparentForMouseEvents, true); // not needed
        warningLabel->setWordWrap (true);
        grid_->addWidget (warningLabel, 1, 0);
        setLayout (grid_);

        if (parent)
        { // compress the bar vertically and show it with animation
            QTimer::singleShot (anotherBar ? DURATION + 10 : 0, this, [this, parent]() {
                parent->installEventFilter (this);
                int h = grid_->minimumHeightForWidth (parent->width()) + grid_->contentsMargins().bottom();
                QRect g (0, parent->height() - h - vOffset_, parent->width(), h);
                setGeometry (g);

                animation_ = new QPropertyAnimation (this, "geometry", this);
                animation_->setEasingCurve (QEasingCurve::Linear);
                animation_->setDuration (DURATION);
                animation_->setStartValue (QRect (0, parent->height() - vOffset_, parent->width(), 0));
                animation_->setEndValue (g);
                animation_->start();
                show();
                connect (animation_, &QAbstractAnimation::finished, this, &WarningBar::shown);
            });
        }
        else show();

        /* auto-close */
        setTimeout (timeout);
    }

    void setTimeout (int timeout) { // can be used to change the timeout
        if (timeout <= 0)
        {
            if (timer_ != nullptr)
            {
                timer_->stop();
                delete timer_;
                timer_ = nullptr;
            }
        }
        else
        {
            if (timer_ == nullptr)
            {
                timer_ = new QTimer (this);
                timer_->setSingleShot (true);
                connect (timer_, &QTimer::timeout, this, [this]() {
                    if (!mousePressed_) closeBar();
                });
            }
            timer_->start (timeout * 1000);
        }
    }

    bool eventFilter (QObject *o, QEvent *e) {
        if (e->type() == QEvent::Resize)
        {
            if (QWidget *w = qobject_cast<QWidget*>(o))
            {
                if (w == parentWidget())
                {
                    if (animation_)
                    {
                        disconnect (animation_, &QAbstractAnimation::finished, this, &WarningBar::shown);
                        animation_->stop();
                        if (isClosing_)
                        {
                            this->deleteLater();
                            return false;
                        }
                    }
                    /* compress the bar as far as its text is shown completely */
                    int h = grid_->minimumHeightForWidth (w->width()) + grid_->contentsMargins().bottom();
                    setGeometry (QRect (0, w->height() - h - vOffset_, w->width(), h));
                }
            }
        }
        return false;
    }

    QString getMessage() const {
        return message_;
    }

    bool isClosing() const {
        return isClosing_;
    }

signals:
    void shown(); // used only with printing (-> FPwin::filePrint)

public slots:
    void closeBar() {
        if (animation_ && parentWidget())
        {
            if (!isClosing_)
            {
                isClosing_ = true;
                disconnect (animation_, &QAbstractAnimation::finished, this, &WarningBar::shown);
                animation_->stop();
                animation_->setStartValue (geometry());
                animation_->setEndValue (QRect (0, parentWidget()->height() - vOffset_, parentWidget()->width(), 0));
                animation_->start();
                connect (animation_, &QAbstractAnimation::finished, this, &QObject::deleteLater);
            }
        }
        else delete this;
    }
    void closeBarOnScrolling (const QRect& /*rect*/, int dy) {
        if (timer_ && dy != 0) closeBar();
    }

protected:
    void mousePressEvent (QMouseEvent *event) {
        QWidget::mousePressEvent (event);
        mousePressed_ = true;
    }
    void mouseReleaseEvent (QMouseEvent *event) {
        QWidget::mouseReleaseEvent (event);
        mousePressed_ = false;
        if (timer_ != nullptr)
            QTimer::singleShot (0, this, &WarningBar::closeBar);
    }

private:
    QString message_;
    int vOffset_;
    bool isClosing_;
    bool mousePressed_;
    QTimer *timer_;
    QGridLayout *grid_;
    QPointer<QPropertyAnimation> animation_;
};

}

#endif // WARNINGBAR_H
