#ifndef DRAGHELPER_H
#define DRAGHELPER_H

#include <QObject>
#include <QStringList>

class IconProvider;

class DragHelper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY dragStateChanged)
    Q_PROPERTY(QStringList activePaths READ activePaths NOTIFY dragStateChanged)

public:
    explicit DragHelper(IconProvider *iconProvider, QObject *parent = nullptr);

    bool active() const;
    QStringList activePaths() const;

    Q_INVOKABLE void startDrag(const QStringList &filePaths,
                               const QString &iconName = QString(),
                               int fileCount = 1);
    Q_INVOKABLE int defaultFileDropAction(const QStringList &paths, const QString &destination) const;
    Q_INVOKABLE int fileDropAction(int defaultAction, int supportedActions) const;

signals:
    void dragStateChanged();
    void dragStarted();
    void dragFinished();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    IconProvider *m_iconProvider;
    QStringList m_activePaths;
    Qt::KeyboardModifiers m_dropModifiers = Qt::NoModifier;
    bool m_active = false;
};

#endif // DRAGHELPER_H
