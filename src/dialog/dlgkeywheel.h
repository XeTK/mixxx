#ifndef DLGKEYWHEEL_H
#define DLGKEYWHEEL_H

#include <QDialog>
#include <QDomDocument>
#include <QSvgWidget>

#include "dialog/ui_dlgkeywheel.h"
#include "track/keyutils.h"

class DlgKeywheel : public QDialog, public Ui::DlgKeywheel {
    Q_OBJECT

  public:
    explicit DlgKeywheel(QWidget* parent, const UserSettingsPointer& pConfig);
    void switchNotation(int dir = 1);
    void updateSvg();
    ~DlgKeywheel() = default;
    void show();

    // The spoken name for a notation, used both to build the notationChanged
    // announcement and by unit tests (issue #63); public for the latter,
    // following the pattern of AnnouncementManager's static formatters.
    static QString notationDisplayName(KeyUtils::KeyNotation notation);

  signals:
    // Emitted whenever the displayed key notation changes, whether via the
    // Up/Down keyboard shortcut or a mouse click on the wheel, so the main
    // window can speak it for screen-reader users (issue #63). Tab/Shift+Tab
    // no longer trigger this: they were freed from notation-cycling duty so
    // they can move keyboard focus normally (e.g. onto the Close button).
    void notationChanged(const QString& notationName);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void resizeEvent(QResizeEvent* ev) override;

  private:
    bool isHiddenNotation(KeyUtils::KeyNotation notation);
    KeyUtils::KeyNotation m_notation;
    QDomDocument m_domDocument;
    const UserSettingsPointer m_pConfig;
    bool m_resized{false};
};

#endif // DLGKEYWHEEL_H
