#pragma once
#include <QColor>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

#include "preferences/usersettings.h"

namespace mixxx {
namespace qml {

class QmlConfigProxy : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Config)
    QML_SINGLETON
  public:
    explicit QmlConfigProxy(
            UserSettingsPointer pConfig,
            QObject* parent = nullptr);

    // We use method here instead of properties as there is no way to achieve property binding
    // with UserSettings, since there is no synchronisation upon mutations.
    Q_INVOKABLE QVariantList getHotcueColorPalette();
    Q_INVOKABLE QVariantList getTrackColorPalette();
    Q_INVOKABLE int getMultiSamplingLevel();

    // Generic bridge for QML preference pages backed by plain ConfigObject
    // key/value settings (bool/int/double/QString). Dispatches on
    // defaultValue's/value's QVariant type to the matching
    // ConfigObject::getValue<T>/setValue<T> specialization. Like the
    // getters above, these are methods rather than a Q_PROPERTY because
    // ConfigObject has no change notification to bind to.
    Q_INVOKABLE QVariant getValue(
            const QString& group,
            const QString& key,
            const QVariant& defaultValue = QVariant());
    Q_INVOKABLE void setValue(
            const QString& group,
            const QString& key,
            const QVariant& value);

    static QmlConfigProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);
    static inline void registerUserSettings(UserSettingsPointer pConfig) {
        s_pUserSettings = std::move(pConfig);
    }

  private:
    static inline UserSettingsPointer s_pUserSettings = nullptr;

    const UserSettingsPointer m_pConfig;
};

} // namespace qml
} // namespace mixxx
