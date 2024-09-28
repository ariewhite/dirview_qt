#include <QApplication>
#include <QFileSystemModel>
#include <QFileIconProvider>
#include <QScreen>
#include <QScroller>
#include <QTreeView>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QDirIterator>
#include <QFileInfo>
#include <QDebug>
#include <QLocale>
#include <QTime>
#include <QStyledItemDelegate>
#include <QPushButton>
#include <QPainter>


/**
 * Метод считает размер директории
 */
qint64 DirSize(const QString &path)
{
    QDirIterator it(path, QDirIterator::Subdirectories);
    qint64 total = 0;

    while (it.hasNext()) {
        it.next();
        if (it.fileInfo().isFile()) {
            total += it.fileInfo().size();
        }
    }

    if (it.fileInfo().isFile()) {
        total += it.fileInfo().size();
    }

    qDebug() << "size: " <<  total;
    return total;
}

/**
 * Переопределенный класс QFileSystemModel.
 * (добавлены обработка для отбражения размера директорий и на 5е поле кнопки обновления размера)
 */
class MyQFileSystemModel : public QFileSystemModel
{
    //Q_OBJECT
public:
    MyQFileSystemModel(QObject *parent = nullptr) : QFileSystemModel(parent) {}

    int columnCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return QFileSystemModel::columnCount(parent) + 1;
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid()) {
            return QVariant();
        }

        QFileInfo fileInfo = QFileSystemModel::fileInfo(index);

        if (index.column() == 0) {
            return QFileSystemModel::data(index, role);
        }

        if (index.column() == 1  && role == Qt::DisplayRole ) {

            if (fileInfo.isDir()) {
                // по дефолту ставим 0
                return QString::number(0) + "byte";
            } else {
                return QString::number(fileInfo.size()) + "byte";
            }
        }

        if (index.column() == 2 && role == Qt::DisplayRole){
            return fileInfo.lastModified().toString(Qt::DateFormat::TextDate);
        }

        if (index.column() == 3 && role == Qt::DisplayRole){
            return fileInfo.isDir() ? "Dir" : fileInfo.suffix();
        }

        return QVariant();
    }
};
/**
 * Делегат для отрисовки кнопки в каждой строке
 */
class BtnDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    BtnDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (index.column() == 4) {
            QStyleOptionButton button;
            button.rect = option.rect;
            button.text = "Update size";
            button.state = QStyle::State_Enabled;

            QStyle *style = QApplication::style();
            style->drawControl(QStyle::CE_PushButton, &button, painter);
        } else {
            QStyledItemDelegate::paint(painter, option, index);
        }
    }

    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override
    {
        if (event->type() == QEvent::MouseButtonPress) {
            if (index.column() == 4) {
                emit buttonClicked(index);
                return true;
            }
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

signals:
    void buttonClicked(const QModelIndex &index) const;
};


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCoreApplication::setApplicationVersion(QT_VERSION_STR);
    QCommandLineParser parser;
    parser.setApplicationDescription("Qt Dir View Example");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption dontUseCustomDirectoryIconsOption("c", "Set QFileSystemModel::DontUseCustomDirectoryIcons");
    parser.addOption(dontUseCustomDirectoryIconsOption);
    QCommandLineOption dontWatchOption("w", "Set QFileSystemModel::DontWatch");
    parser.addOption(dontWatchOption);
    parser.addPositionalArgument("directory", "The directory to start in.");
    parser.process(app);
    // указываем путь до папки юзера
    // linux: /home/*user*/
    // windows: C:/Users/*user*/
    const QString rootPath = QDir::homePath();

    MyQFileSystemModel model;
    model.setRootPath("");
    if (parser.isSet(dontUseCustomDirectoryIconsOption))
        model.setOption(QFileSystemModel::DontUseCustomDirectoryIcons);
    if (parser.isSet(dontWatchOption))
        model.setOption(QFileSystemModel::DontWatchForChanges);

    model.setFilter(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden);
    model.setNameFilterDisables(false);

    QTreeView tree;
    tree.setModel(&model);
    if (!rootPath.isEmpty()) {
        const QModelIndex rootIndex = model.index(QDir::cleanPath(rootPath));
        if (rootIndex.isValid())
            tree.setRootIndex(rootIndex);
    }

    BtnDelegate *bd = new BtnDelegate(&tree);
    tree.setItemDelegateForColumn(4, bd);

    QObject::connect(bd, &BtnDelegate::buttonClicked, [&model, &tree](const QModelIndex &index){
        if (!index.isValid()) return;

        QFileInfo file = model.fileInfo(index);

        if (file.isDir()){
            qint64 size = DirSize(file.absoluteFilePath());
            model.setData(index.siblingAtColumn(1), QString::number(size) + " bytes", Qt::DisplayRole);
            emit model.dataChanged(index.siblingAtColumn(1), index.siblingAtColumn(1));

            QModelIndex indexUp = index.siblingAtColumn(1);
            tree.update(indexUp);
        }
    });

    tree.setAnimated(false);
    tree.setIndentation(20);
    tree.setSortingEnabled(true);
    const QSize availableSize = tree.screen()->availableGeometry().size();
    tree.resize(availableSize / 2);
    tree.setColumnWidth(0, tree.width() / 3);
    tree.setColumnWidth(1, 100);

    QScroller::grabGesture(&tree, QScroller::TouchGesture);

    QWidget window;
    window.setWindowTitle(QObject::tr("Dir View"));
    window.resize(availableSize / 2);

    QLineEdit *edit = new QLineEdit();
    edit->setPlaceholderText("Input dir name...");

    QObject::connect(edit, &QLineEdit::textChanged, [&model, edit]() {
        if (!edit->text().isEmpty()) {
            model.setNameFilters(QStringList({ edit->text() }));
        } else {
            model.setNameFilters(QStringList({ QString("*") }));
        }
    });

    QVBoxLayout mainLayout;
    mainLayout.addWidget(edit);
    mainLayout.addWidget(&tree);
    window.setLayout(&mainLayout);
    window.show();

    return app.exec();
}

#include "main.moc"
