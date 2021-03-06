/* Copyright (C) 2004-2008 Daniel Vйritй

   This file is part of Manitou-Mail (see http://www.manitou-mail.org)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <string>
#include <iostream>
#include <ctype.h>
#include <stdlib.h>
#include <QMessageBox>
#include <QDateTime>
#include <QDebug>

#include "db.h"
#include "connection.h"
#include "main.h"
#include "SQLite/sqlite_stream.h"

//================================== db_excpt ====================================//
db_excpt::db_excpt(const QString query,
			 const QString msg,
			 QString code/*=QString::null*/)
{
  m_query=query;
  m_err_msg=msg;
  m_err_code=code;
  DBG_PRINTF(3, "db_excpt: query='%s', err='%s'",
             m_query.toLocal8Bit().constData(), m_err_msg.toLocal8Bit().constData());
}

db_excpt::db_excpt(const QString query, db_cnx& d)
{
  m_query=query;
  const char* pg_msg = sqlite3_errmsg(d.connection()->m_db->connection());
  if (pg_msg!=NULL)
    m_err_msg = QString::fromUtf8(pg_msg);
}

void DBEXCPT(db_excpt& p)
{
  //  std::cerr << p.query() << ":" << p.errmsg() << std::endl;
  QString err = p.query() + ":\n";
  err+=p.errmsg();
  QMessageBox::warning(NULL, QObject::tr("Database error"), err);
}
//___________________________________db_excpt______________________________________//


//================================== sql_stream ====================================//
//***************** Callback functions
/*int
sql_stream::callback (void* voidThis, int count, char** values, char** columnNames)
{
  sql_stream* This = static_cast<sql_stream*> (voidThis);
  return This->callback(count, values, columnNames);
}

int
sql_stream::callback(int count, char** values, char** columnNames)
{
  m_callback = true;
  if (count == 0)
    return SQLITE_ERROR;
  Rec rec;
  for (int i=0; i<count; ++i)
  {
    rec.insert(columnNames[i], values[i]);
  }
  m_resultData.append(rec);
  return 0;
}
*/
//======================== public ===============================
namespace service_f {
  void replace_random_param (QString &str)
  {
    str = str.replace("%", "\1");
  }
  void return_random_param (QString &str)
  {
    str = str.replace("\1", "%");
  }
}

sql_stream::sql_stream (const QString query, db_cnx& db) :
  m_db(db), m_query(query), m_nArgPos(0), m_nArgCount(0),
  m_bExecuted(false), m_sqlRes(NULL)
{
  service_f::replace_random_param(m_query);
  find_key_word();
  find_param();

  if(m_nArgCount == 0)
    execute();
}

sql_stream::~sql_stream()
{
  if (m_sqlRes)
    sqlite3_finalize(m_sqlRes);
  if(!m_bExecuted)
  {
#ifdef QT_DEBUG
    throw db_excpt(m_query, "QUERY NOT EXECUTE!");
#else
    DBG_PRINTF(1, "QUERY NOT EXECUTE!");
#endif
  }
}


// operators <<
sql_stream&
sql_stream::operator<<(const QString &s)
{
  return next_param('\'' + s + '\'');
}

sql_stream&
sql_stream::operator<<(const char* s)
{
  return next_param('\'' + QString(s) + '\'');
}

sql_stream&
sql_stream::operator<<(sql_null n _UNUSED_)
{
  return next_param(QString("null"));
}

//operators >>
sql_stream&
sql_stream::operator>>(int& i)
{
  i = next_result().toInt();
  return *this;
}

sql_stream&
sql_stream::operator>>(unsigned int& i)
{
  i = next_result().toUInt();
  return *this;
}

sql_stream&
sql_stream::operator>>(char& c)
{
  c = next_result().at(0).cell();
  return *this;
}

sql_stream&
sql_stream::operator>>(char* p)
{
  strcpy(next_result().toLocal8Bit().data(), p);
  return *this;
}

sql_stream&
sql_stream::operator>>(QString& s)
{
  s = next_result();
  return *this;
}
//______________end operators

void
sql_stream::execute()
{
  if (m_nArgPos < m_nArgCount)
    throw db_excpt(m_query, "Not all variables bound");

  DBG_PRINTF(5,"execute: %s", m_query.toLocal8Bit().constData());

  service_f::return_random_param(m_query);
  const char *errmsg = 0;
  int rc = 0;
  bool isLocked = false;
  do
  {
    rc = sqlite3_prepare(m_db.connection()->m_db->connection(),
                         m_query.toLocal8Bit().constData(),
                         -1, &m_sqlRes, &errmsg);
    m_status = sqlite3_step(m_sqlRes);
    isLocked = (rc == SQLITE_BUSY) || (m_status == SQLITE_LOCKED);
    if (isLocked)
      sleep(1);
  } while (isLocked);
  check_results(rc, errmsg);
  m_affected_rows = sqlite3_changes(m_db.connection()->m_db->connection());

  m_rowNumber=0;
  m_colNumber=0;
  m_bExecuted=true;
}

int
sql_stream::isEmpty()
{
  if (!m_bExecuted)
    execute();

  if (m_colNumber+1 < sqlite3_data_count(m_sqlRes))
    return false;
  return m_status == SQLITE_DONE;
}

int
sql_stream::affected_rows() const
{
  return m_affected_rows;
}


//======================== private ===============================


void
sql_stream::find_key_word()
{
  QString sQuery = m_query;
  const QString NOW = ":now:";
  int pos_now = sQuery.indexOf(NOW);
  if (pos_now != -1)
  {
    sQuery = sQuery.replace(NOW,
                            '\'' + QDateTime::currentDateTime().
                                   toString("dd.MM.yyyy hh::mm::ss.zzzz")
                            + '\'');
  }
  m_query = sQuery;
}

void
sql_stream::find_param()
{
  QString sQuery = m_query;
  int pos = sQuery.indexOf(':');
  while (pos != -1)
  {
    if (sQuery.at(pos+1) != ':')
    {
      int rightPos = pos;
      while ((++rightPos < sQuery.size()) && sQuery.at(rightPos).isLetterOrNumber());
      int replaceCount = rightPos - pos;
      if (replaceCount > 1) { //length variable > 1
        sQuery.replace(pos, replaceCount,
                       "%" + QString::number(m_nArgCount));
        ++m_nArgCount;
      }
      else
        pos += 1;
    }
    else
      pos += 2;
    pos = sQuery.indexOf(':', pos);
  }
  m_query = sQuery;
}

void
sql_stream::check_params() const
{
  if (m_nArgPos >= m_nArgCount)
    throw db_excpt(m_query, "Mismatch between bound variables and query");
}

sql_stream&
sql_stream::next_param(QString value)
{
  check_params();
  m_query = m_query.arg(value);
  ++m_nArgPos;
  return *this;
}

void
sql_stream::check_end_of_stream()
{
  if (isEmpty())
    throw db_excpt(m_query, "End of stream reached");
}

QString
sql_stream::next_result()
{
  QString result;
  check_end_of_stream();
  if (m_status == SQLITE_ROW)
  {
    result = QString(((const char*)sqlite3_column_text(m_sqlRes, m_colNumber)));

    increment_position();
  }
  return result;
}

void
sql_stream::check_results(int code_error, const QString& errmsg)
{
  if (m_status == SQLITE_MISUSE)
    throw db_excpt(m_query, m_db);

  if (code_error != SQLITE_OK)
    throw db_excpt(m_query, errmsg, QString::number(code_error));
}

void
sql_stream::increment_position()
{
  if (m_status != SQLITE_DONE)
  {
    m_colNumber = ((m_colNumber+1) % sqlite3_column_count(m_sqlRes));
    if (m_colNumber==0)
    {
      m_rowNumber++;
      m_status = sqlite3_step(m_sqlRes);
    }
  }
}
