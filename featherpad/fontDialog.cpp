/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2019-2021 <tsujan2000@gmail.com>
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

#include "fontDialog.h"
#include "ui_fontDialog.h"

namespace FeatherPad {

FontDialog::FontDialog (const QFont &font, QWidget *parent)
    : QDialog (parent),
      ui (new Ui::FontDialog),
      font_ (font)
{
    ui->setupUi (this);
    int widthHint = sizeHint().width(); // before hiding one of combo boxes

    /* a coding font should be normal and have a fixed pitch */
    bool codingFont = !font_.italic() && font_.weight() == QFont::Normal
                      && QFontInfo (font_).fixedPitch();

    ui->monoFontComboBox->setCurrentFont (font_); // may select another font
    ui->fontComboBox->setCurrentFont (font_);
    if (codingFont)
    {
        ui->codingFontBox->setChecked (true);
        ui->fontComboBox->hide();
        ui->fontLabel->hide();
        ui->monoFontComboBox->show();
        ui->monoFontLabel->show();
    }
    else
    {
        ui->codingFontBox->setChecked (false);
        ui->monoFontComboBox->hide();
        ui->monoFontLabel->hide();
        ui->fontComboBox->show();
        ui->fontLabel->show();

        ui->italicBox->setChecked (font_.italic());

        int weight = font_.weight();
        switch (weight) {
        case QFont::Normal:
            ui->weightComboBox->setCurrentIndex (0);
            break;
        case QFont::Medium:
            ui->weightComboBox->setCurrentIndex (1);
            break;
        case QFont::Bold:
            ui->weightComboBox->setCurrentIndex (2);
            break;
        case QFont::Black:
            ui->weightComboBox->setCurrentIndex (3);
            break;
        default:
            if (weight < QFont::Normal)
            {
                font_.setWeight (QFont::Normal);
                ui->weightComboBox->setCurrentIndex (0);
            }
            else if (weight < QFont::Medium)
            {
                font_.setWeight (QFont::Medium);
                ui->weightComboBox->setCurrentIndex (1);
            }
            else if (weight < QFont::Bold)
            {
                font_.setWeight (QFont::Bold);
                ui->weightComboBox->setCurrentIndex (2);
            }
            else
            {
                font_.setWeight (QFont::Black);
                ui->weightComboBox->setCurrentIndex (3);
            }
            break;
        }
    }

    ui->lineEdit->setFont (font_);
    ui->spinBox->setValue (font_.pointSize());

    connect (ui->fontComboBox, &QFontComboBox::currentFontChanged, [this] (const QFont &curFont) {
        int fontSize = font_.pointSize();
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
        int weight = font_.weight();
#else
        QFont::Weight weight = font_.weight();
#endif
        bool italic = font_.italic();
        font_ = curFont;
        font_.setPointSize (fontSize);
        font_.setWeight (weight);
        font_.setItalic (italic);
        ui->lineEdit->setFont (font_);
    });
    connect (ui->monoFontComboBox, &QFontComboBox::currentFontChanged, [this] (const QFont &curFont) {
        int fontSize = font_.pointSize();
        font_ = curFont;
        font_.setPointSize (fontSize);
        ui->lineEdit->setFont (font_);
    });

    connect (ui->spinBox, QOverload<int>::of(&QSpinBox::valueChanged), [this] (int value) {
        font_.setPointSize (value);
        ui->lineEdit->setFont (font_);
    });

    connect (ui->codingFontBox, &QCheckBox::stateChanged, [this] (int checked) {
        int fontSize = font_.pointSize();

        if (checked == Qt::Checked)
        {
            ui->fontComboBox->hide();
            ui->fontLabel->hide();
            ui->monoFontComboBox->show();
            ui->monoFontLabel->show();

            font_ = ui->fontComboBox->currentFont();
            if (!QFontInfo (font_).fixedPitch())
                font_ = ui->monoFontComboBox->currentFont();
        }
        else if (checked == Qt::Unchecked)
        {
            ui->monoFontComboBox->hide();
            ui->monoFontLabel->hide();
            ui->fontComboBox->show();
            ui->fontLabel->show();

            font_ = ui->monoFontComboBox->currentFont();
        }

        ui->italicBox->setChecked (false);
        ui->weightComboBox->setCurrentIndex (0);

        font_.setPointSize (fontSize);
        font_.setWeight (QFont::Normal);
        font_.setItalic (false);
        ui->monoFontComboBox->setCurrentFont (font_);
        ui->fontComboBox->setCurrentFont (font_);
        ui->lineEdit->setFont (font_);
    });

    connect (ui->italicBox, &QCheckBox::stateChanged, [this] (int checked) {
        if (checked == Qt::Checked)
            font_.setItalic (true);
        else if (checked == Qt::Unchecked)
            font_.setItalic (false);
        ui->lineEdit->setFont (font_);
    });

    connect (ui->weightComboBox, QOverload<const int>::of(&QComboBox::currentIndexChanged), [this] (int index) {
        switch (index) {
        case 0:
            font_.setWeight (QFont::Normal);
            break;
        case 1:
            font_.setWeight (QFont::Medium);
            break;
        case 2:
            font_.setWeight (QFont::Bold);
            break;
        case 3:
            font_.setWeight (QFont::Black);
            break;
        default:
            break;
        }
        ui->lineEdit->setFont (font_);
    });

    resize (QSize (widthHint, sizeHint().height())); // show it compact
}
/*************************/
FontDialog::~FontDialog()
{
    delete ui; ui = nullptr;
}

}
