#ifndef REGISTRYTEST_H
#define REGISTRYTEST_H
#include <cppunit/extensions/HelperMacros.h>

namespace TestNames
{
    std::string db_api();
    std::string connect();
    std::string disconnect();
    std::string sql_wirte_fields();
};







//этот тест должен быть первым в DB
CPPUNIT_REGISTRY_ADD(TestNames::connect(), TestNames::db_api());
CPPUNIT_REGISTRY_ADD(TestNames::sql_wirte_fields(), TestNames::db_api());
//этот тест должен быть последний DB
CPPUNIT_REGISTRY_ADD("Disconnect", TestNames::db_api());


#endif // REGISTRYTEST_H