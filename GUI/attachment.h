/* Copyright (C) 2004-2008 Daniel V�rit�

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

#ifndef INC_ATTACHMENT_H
#define INC_ATTACHMENT_H

#include <QMap>
#include <fstream>
#include "dbtypes.h"
#include "database.h"
#include "sqlstream.h"
#include <QStringList>
#include <QNetworkReply>
#include <QNetworkRequest>

class attachment_network_reply;

class attachment
{
public:
  struct lo_ctxt {
    db_cnx* db;
    int lfd;
    unsigned int size;
    unsigned int chunk_size;
    bool eof;
  };
  attachment();
  attachment(const attachment&);
  virtual ~attachment();
  mail_id_t getId() const { return m_Id; }
  void setId(mail_id_t id) { m_Id=id; }

  void compute_sha1_fp();

  /* Allocate m_data if needed and fetch contents into m_data */
  char* get_contents();

  void get_size_from_file();

  int open_lo(struct lo_ctxt*);
  void close_lo(struct lo_ctxt*);
  int streamout_chunk(struct lo_ctxt* slo, std::ofstream& of);

  void streamout_content(std::ofstream&);
  bool store(uint mail_id);

  /* Insert the contents of a file into the ATTACHMENT_CONTENTS table
     members updated: m_size, m_Id */
  bool import_file_content();

  static QString guess_mime_type(const QString);
  uint size() { fetch(); return m_size; }
  const QString filename() { fetch(); return m_filename; }
  QString mime_type() { fetch(); return m_mime_type; }
  QString charset() { fetch(); return m_charset; }
  QString application() const;
  void launch_external_viewer(const QString document_path);

  attachment_network_reply* network_reply(const QNetworkRequest& req, QObject* parent);

  // returns the full path and name suitable to store a temporary
  // copy of the attachment 
  QString get_temp_location();

  void setAll(mail_id_t id, uint size, const QString filename, const QString mime_type,
    const QString charset=QString::null) {
    m_Id=id;
    m_filename=filename;
    m_size=size;
    m_mime_type=mime_type;
    m_charset=charset;
    m_descFetched=true;
  }
  void set_mime_content_id(const QString id) {
    m_mime_content_id=id;
  }
  const QString mime_content_id() const {
    return m_mime_content_id;
  }
  void create_mime_content_id();

  void set_filename(const QString filename) {
    m_filename=filename;
  }
  void set_mime_type(const QString mimetype) {
    m_mime_type = mimetype;
  }
  // allocate and copy the contents
  void set_contents(const char* contents, uint size);

  // append to 'body' the contents of the attachment. May include a conversion
  // of the attachment contents to unicode.
  void append_decoded_contents(QString& body);

  // don't allocate or copy the contents
  void set_contents_ptr(char* contents, uint size);

  // returns true if the contents include non-ASCII characters
  bool is_binary();


  // functions for streaming contents when driven by attachment_network_reply
  // instantiated by webkit
  bool open();
  qint64 read(qint64 size, char* buf);
  bool eof() const;
  void close();

  static bool fetch_filename_suffixes(QMap<QString,QString>&);

protected:
  mail_id_t m_Id;
  char *m_data;			// with malloc() and free()
  QString m_filename;
  QString m_charset;
  QString m_mime_type;
  QString m_mime_content_id;
  uint m_size;
  // "description" of attachment has been fetched or is otherwise known
  bool m_descFetched;
  bool m_inMemory;
  QString m_sha1_b64;
private:
  QString sha1_to_base64(unsigned int digest[5]);
  // dummy
  bool fetch();
  void free_data();
  struct lo_ctxt m_lo;
};

class attachment_network_reply : public QNetworkReply
{
  Q_OBJECT
public:
  attachment_network_reply(const QNetworkRequest &req, attachment* a,
			   QObject* parent);
  virtual ~attachment_network_reply();
  qint64 readData(char* data, qint64 size);
  void abort();
  qint64 bytesAvailable() const;
  bool atEnd() const;
  bool isSequential() const;
private:
  attachment* m_a;
  int already_read;
private slots:
  void go();
};

class attachments_list : public std::list<attachment>
{
public:
  attachments_list();
  virtual ~attachments_list();
  bool fetch();
  bool store();
  void setMailId(mail_id_t id) { m_mailId=id; }
  attachment* get_by_content_id(const QString mime_content_id);

private:
  uint m_mailId;
  bool m_bFetched;
};

class attachment_viewer {
public:
  attachment_viewer();
  virtual ~attachment_viewer();
  QString m_mime_type;
  QStringList m_suffixes;	// not used yet
  QString m_program;
};

class attch_viewer_list : public std::list<attachment_viewer>
{
public:
  attch_viewer_list();
  virtual ~attch_viewer_list();
  bool fetch(const QString& conf_name, bool force=false);
private:
  bool m_fetched;
};

#endif
