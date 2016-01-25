#ifndef MODEL_H
#define MODEL_H

#include <QAbstractTableModel>

namespace Mediathek
{

class Database;

class Model : public QAbstractTableModel
{
    Q_OBJECT
    Q_DISABLE_COPY(Model)

public:
    Model(Database& database, QObject* parent = 0);
    ~Model();

public:
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent) const override;

    QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;

    void filter(const QString& channel, const QString& topic, const QString& title);
    void sort(int column, Qt::SortOrder order) override;

protected:
    bool canFetchMore(const QModelIndex& parent) const override;
    void fetchMore(const QModelIndex& parent) override;

public:
    QStringList channels() const;
    QStringList topics() const;
    QStringList topics(const QString& channel) const;

    QString description(const QModelIndex& index) const;
    QUrl website(const QModelIndex& index) const;

public slots:
    void reset();

private:
    const Database& m_database;

    QVector< quintptr > m_id;
    int m_fetched = 0;

    QString m_channel;
    QString m_topic;
    QString m_title;

    int m_sortColumn = -1;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;

    void update();

};

} // Mediathek

#endif // MODEL_H
