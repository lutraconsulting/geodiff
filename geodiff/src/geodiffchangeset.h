/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#ifndef GEODIFFCHANGESET_H
#define GEODIFFCHANGESET_H

#include <assert.h>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "geodiff.h"


/** Representation of a single value stored in a column.
 * It can be one of types:
 * - NULL
 * - integer
 * - double
 * - string
 * - binary data (blob)
 *
 * There is also a special "undefined" value type which is different
 * from "null". The "undefined" value means that the particular value
 * has not changed, for example in UPDATE change if a column's value
 * is unchanged, its value will have this type.
 */
struct Value
{
    //! Possible value types
    enum Type
    {
      TypeUndefined = 0,   //!< equal to "undefined" value type in sqlite3 session extension
      TypeInt       = 1,   //!< equal to SQLITE_INTEGER
      TypeDouble    = 2,   //!< equal to SQLITE_FLOAT
      TypeText      = 3,   //!< equal to SQLITE_TEXT
      TypeBlob      = 4,   //!< equal to SQLITE_BLOB
      TypeNull      = 5,   //!< equal to SQLITE_NULL
    };

    Type type() const { return mType; }

    int64_t getInt() const
    {
      assert( mType == TypeInt );
      return mVal.num_i;
    }
    double getDouble() const
    {
      assert( mType == TypeDouble );
      return mVal.num_f;
    }
    const std::string &getString() const
    {
      assert( mType == TypeText || mType == TypeBlob );
      return *mVal.str;
    }

    void setInt( int64_t n )
    {
      reset();
      mType = TypeInt;
      mVal.num_i = n;
    }
    void setDouble( double n )
    {
      reset();
      mType = TypeDouble;
      mVal.num_f = n;
    }
    void setString( Type t, const char *ptr, int size )
    {
      reset();
      assert( t == TypeText || t == TypeBlob );
      mType = t;
      mVal.str = new std::string( ptr, size );
    }
    void setUndefined()
    {
      reset();
    }
    void setNull()
    {
      reset();
      mType = TypeNull;
    }

    Value() {}
    ~Value() { reset(); }

    Value( const Value &other )
    {
      mType = other.mType;
      mVal = other.mVal;
      if ( mType == TypeText || mType == TypeBlob )
      {
        mVal.str = new std::string( *mVal.str ); // make a deep copy
      }
    }
    Value &operator=( const Value &other ) = delete; // for now just to be sure

  protected:
    void reset()
    {
      if ( mType == TypeText || mType == TypeBlob )
      {
        delete mVal.str;
      }
      mType = TypeUndefined;
    }

  protected:
    Type mType = TypeUndefined;
    union
    {
      int64_t num_i;
      double num_f;
      std::string *str;
    } mVal;

};


/**
 * Table metadata stored in changeset file
 */
struct ChangesetTable
{
  //! Name of the table
  std::string name;
  //! Array of true/false values (one for each column) - indicating whether particular column is a part of primary key
  std::vector<bool> primaryKeys;
};


/**
 * Details of a single change within a changeset
 *
 * Contents of old/new values array based on operation type:
 * - INSERT - new values contain data of the row to be inserted, old values array is invalid
 * - DELETE - old values contain data of the row to be deleted, new values array is invalid
 * - UPDATE - both old and new values arrays are valid, if a column has not changed, both
 *            old and new value have "undefined" value type. In addition to that, primary key
 *            columns of old value are always present (but new value of pkey columns is undefined
 *            if the primary key is not being changed).
 */
struct ChangesetEntry
{
  enum OperationType
  {
    OpInsert = 18,  //!< equal to SQLITE_INSERT
    OpUpdate = 23,  //!< equal to SQLITE_UPDATE
    OpDelete = 9,   //!< equal to SQLITE_DELETE
  };

  //! Type of the operation in this entry
  OperationType op;
  //! Column values for "old" record - only valid for UPDATE and DELETE
  std::vector<Value> oldValues;
  //! Column values for "new" record - only valid for UPDATE and INSERT
  std::vector<Value> newValues;
  ChangesetTable *table = nullptr;
};


class Buffer;

/**
 * Class for reading of binary changeset files.
 * First use open() to initialize it, followed by a series of nextEntry() calls.
 */
class GEODIFF_EXPORT GeoDiffChangesetReader
{
  public:
    GeoDiffChangesetReader();
    ~GeoDiffChangesetReader();

    //! Starts reading of changeset from a file
    bool open( const std::string &filename );

    //! Reads next changeset entry to the passed object
    bool nextEntry( ChangesetEntry &entry );

  private:

    char readByte();
    int readVarint();
    std::string readNullTerminatedString();
    void readRowValues( std::vector<Value> &values );
    void readTableRecord();

    void throwReaderError( const std::string &message );

    int offset = 0;  // where are we in the buffer

    std::unique_ptr<Buffer> buffer;

    ChangesetTable currentTable;  // currently processed table
};


/**
 * Class for writing binary changeset files.
 * First use open() to create a new changeset file and then for each modified table:
 * - call beginTable() once
 * - then call writeEntry() for each change within that table
 */
class GEODIFF_EXPORT GeoDiffChangesetWriter
{
  public:

    //! opens a file for writing changeset (will overwrite if it exists already)
    bool open( const std::string &filename );

    //! writes table information, all subsequent writes will be related to this table until next call to beginTable()
    void beginTable( const ChangesetTable &table );

    //! writes table change entry
    void writeEntry( const ChangesetEntry &entry );

  private:

    void writeByte( char c );
    void writeVarint( int n );
    void writeNullTerminatedString( const std::string &str );

    void writeRowValues( const std::vector<Value> &values );

    std::ofstream myfile;

    ChangesetTable currentTable;  // currently processed table
};

#endif // GEODIFFCHANGESET_H