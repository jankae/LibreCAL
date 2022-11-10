#ifndef TOUCHSTONEIMPORTDIALOG_H
#define TOUCHSTONEIMPORTDIALOG_H

#include "touchstone.h"

#include <QDialog>

namespace Ui {
class TouchstoneImportDialog;
}

class TouchstoneImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TouchstoneImportDialog(int ports, QString initialFile);
    ~TouchstoneImportDialog();

signals:
    void fileImported(Touchstone t);

private:
    Ui::TouchstoneImportDialog *ui;
};

#endif // TOUCHSTONEIMPORTDIALOG_H
