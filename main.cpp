#include <QBitArray>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QSharedPointer>
#include <QTextStream>

void writeToFile(const QString& id, const qreal& active_time,
                 const qreal& stop_time)
{
    QFile file("out.txt");
    if (!file.open(QIODevice::Append | QIODevice::Text))
        qCritical() << "Cannot found " << file.fileName() << " file";

    QTextStream out(&file);

    out << "---\n";
    out << "id: " << id << "\n";
    out << QString("время в пути: ") << QString::number(active_time, 'f', 2) <<
           "h\n";
    out << QString("время стоянки: ") << QString::number(stop_time, 'f', 2) <<
           "h\n";

    file.flush();
    file.close();
}

bool readAllData(QVector<QString>& id_list,
                 QList<QSharedPointer<QBitArray>>& time_track_list)
{
    // mask of time for parse log.csv
    QString time_mask("yyyy-MM-dd hh:mm:ss");

    QFile file("log.csv");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qCritical() << "Cannot found" << file.fileName() << "file";
        return false;
    }

    while(!file.atEnd())
    {
        QStringList list = QString(file.readLine()).split(',');
        QTime time = QDateTime::fromString(list.at(0), time_mask).time();
        if (!time.isValid())
            continue;

        int seconds = 3600*time.hour() + 60*time.minute() + time.second();

        auto id = list.at(1);
        int track_index = id_list.indexOf(id);

        if ( track_index == -1 )
        {
            // size - seconds in 24 hours
            auto p = QSharedPointer<QBitArray>(new (std::nothrow) QBitArray(60*60*24));
            if (p)
            {
                id_list.append(id);
                time_track_list.append(p);
                track_index = time_track_list.size() - 1;
            }
            else
                qWarning() << "memory cannot be allocated";
        }
        (*time_track_list[track_index])[seconds] = list.at(2).toInt() > 0 ? true : false;
    }

    file.close();
    return true;
}

uint calculateActiveTime(const QSharedPointer<QBitArray> track_time)
{
    // all times in seconds
    uint32_t active_time = 0, stop_time = 0;
    constexpr uint8_t two_minutes = 60*2;

    //  for(auto &item: *track_time)
    for(auto i = 0; i < track_time->size(); ++i)
    {
        if ( (*track_time)[i] )
        {
            // parking time < 2 minutes
            if (stop_time != 0 && stop_time < two_minutes)
            {
                active_time += stop_time;
                stop_time = 0;
            }
            else
                stop_time = 0;

            ++active_time;
        }
        else
            ++stop_time;
    }
    return active_time;
}

int main()
{
    qDebug() << QT_VERSION_STR;

    QFile file("out.txt");
    file.remove();

    // data from time_track_list get by id index from id_list
    QVector<QString> id_list;
    QList<QSharedPointer<QBitArray>> time_track_list;

    // in seconds
    constexpr uint16_t hour = 3600;
    // in hours
    constexpr qreal monitoring_interval = 24.0;

    if (!readAllData(id_list, time_track_list))
        return 1;

    for(int i = 0; i < id_list.size(); ++i)
    {
        uint active_time_sec = calculateActiveTime(time_track_list[i]);

        qreal active_time_hour = static_cast<qreal>(active_time_sec)/hour;
        qreal stop_time_hour = monitoring_interval - active_time_hour;
        writeToFile(id_list[i], active_time_hour, stop_time_hour);
    }

    qDebug() << "Complete";
    return 0;
}
