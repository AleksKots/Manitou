#include <QDebug>
#include "testSqlStream.h"
#include "RegistryTest.h"
#include "sqlstream.h"

std::string TestNames::sql_stream() { return "Sql Stream";}
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(testSqlStream, TestNames::sql_stream());


void testSqlStream::empty()
{
    sql_stream empty("SELECT * FROM test_table where test_int_not_null='0'", *m_DB);
    CPPUNIT_ASSERT(empty.eof());
}
void testSqlStream::noEmpty()
{
    InsertString_();
    InsertString_();
    sql_stream empty("SELECT * FROM test_table", *m_DB);
    CPPUNIT_ASSERT(!empty.eof());
}

void testSqlStream::withVariable()
{
    try {
        InsertString_();
        sql_stream query("SELECT :p1 FROM :p2", *m_DB);
        query << "test_text" << "test_table";
        QString text;
        query >> text;
        CPPUNIT_ASSERT (text == "Test Text");
    }
    catch(db_excpt e)
    {
        DebugExept(e);
    }
}
