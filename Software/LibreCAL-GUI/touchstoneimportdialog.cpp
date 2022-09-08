#include "touchstoneimportdialog.h"
#include "ui_touchstoneimportdialog.h"

#include <QPushButton>

TouchstoneImportDialog::TouchstoneImportDialog(int ports, QString initialFile) :
    QDialog(nullptr),
    ui(new Ui::TouchstoneImportDialog)
{
    ui->setupUi(this);
    ui->touchstoneImport->setPorts(ports);

    ui->touchstoneImport->setFile(initialFile);
    connect(ui->touchstoneImport, &TouchstoneImport::statusChanged, ui->buttonBox->button(QDialogButtonBox::Ok), &QPushButton::setEnabled);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, [=](){
        emit fileImported(ui->touchstoneImport->getTouchstone());
    });
}

TouchstoneImportDialog::~TouchstoneImportDialog()
{
    delete ui;
}
