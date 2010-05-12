#include "kstarsdb.h"


int main() {
    
    KStarsDB* kdb = KStarsDB::Create();
    kdb->createDefaultDatabase(QString("kstars.db"));

    delete kdb;
    
    return 0;
}
