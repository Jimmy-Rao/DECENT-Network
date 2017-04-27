/*
 *	File: decent_wallet_ui_gui_newcheckbox.hpp
 *
 *	Created on: 22 Feb 2017
 *	Created by: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *  This file implements ...
 *
 */
#ifndef DECENT_BUTTON_HPP
#define DECENT_BUTTON_HPP

#include <QLabel>
#include <QPushButton>

namespace gui_wallet {

class DecentButton : public QLabel
{
    Q_OBJECT
public:
    DecentButton();
   
   void setEnabled(bool isEnabled);
   bool isEnabled() const { return _isEnabled; }
    
protected:
    void mousePressEvent(QMouseEvent *event) {
       if (_isEnabled)
          emit LabelClicked();
    }
   
public:
signals:
    void LabelClicked();
   
private:
   bool _isEnabled;
};
   
   
   
class DecentButton1 : public QPushButton
{
   Q_OBJECT
public:
   DecentButton1(QWidget *parent = Q_NULLPTR);
};

}

#endif // DECENT_BUTTON_HPP
