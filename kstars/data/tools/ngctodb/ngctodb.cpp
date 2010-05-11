#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QtDebug>



int main() {
	QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
	db.setHostName("NGCServer");
	db.setDatabaseName("ngcdb");

	bool ok = db.open();
	QSqlQuery query(db);

	qDebug() << query.lastError().text();
	query.exec();
	db.close();
	return 0;
}
