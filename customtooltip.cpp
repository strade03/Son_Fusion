#include "customtooltip.h"
#include <QToolTip>

CustomTooltip::CustomTooltip(QWidget* widget, const QString& text) {
    widget->setToolTip(text);
}
