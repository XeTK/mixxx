#pragma once
#include <QHash>
#include <QIdentityProxyModel>
#include <QQmlEngine>
#include <QUrl>

#include "track/trackid.h"

class LibraryTableModel;

namespace mixxx {
namespace qml {

class QmlLibraryTrackListModel : public QIdentityProxyModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryTrackListModel)
    QML_UNCREATABLE("Only accessible via Mixxx.Library.model")

  public:
    enum Roles {
        TitleRole = Qt::UserRole,
        ArtistRole,
        AlbumRole,
        AlbumArtistRole,
        FileUrlRole,
        DurationRole,
        BpmRole,
        KeyRole,
        GenreRole,
        CoverArtUrlRole,
    };
    Q_ENUM(Roles);

    QmlLibraryTrackListModel(LibraryTableModel* pModel, QObject* pParent = nullptr);
    ~QmlLibraryTrackListModel() = default;

    QVariant data(const QModelIndex& index, int role) const override;
    int columnCount(const QModelIndex& index = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QVariant get(int row) const;

  private:
    // Resolving cover art requires hydrating a full Track object and
    // probing for embedded/sidecar artwork (see data()'s CoverArtUrlRole
    // case) - too expensive to redo on every data() call for a role that
    // never changes for a given track while the library list is open.
    mutable QHash<TrackId, QUrl> m_coverArtUrlCache;
};

} // namespace qml
} // namespace mixxx

Q_DECLARE_METATYPE(mixxx::qml::QmlLibraryTrackListModel*)
