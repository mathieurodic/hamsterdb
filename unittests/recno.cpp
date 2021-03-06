/*
 * Copyright (C) 2005-2015 Christoph Rupp (chris@crupp.de).
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "3rdparty/catch/catch.hpp"

#include <limits>

#include "utils.h"
#include "os.hpp"

#include "3btree/btree_index.h"
#include "3page_manager/page_manager.h"
#include "4context/context.h"
#include "4db/db_local.h"
#include "4env/env_local.h"

namespace hamsterdb {

template<typename RecnoType>
class RecordNumberFixture
{
  uint32_t m_flags;
  ham_db_t *m_db;
  ham_env_t *m_env;
  ScopedPtr<Context> m_context;

public:
  RecordNumberFixture(uint32_t flags = 0)
    : m_flags(flags) {
    REQUIRE(0 ==
        ham_env_create(&m_env, Utils::opath(".test"), m_flags, 0664, 0));
    if (sizeof(RecnoType) == 4)
      REQUIRE(0 == ham_env_create_db(m_env, &m_db, 1, HAM_RECORD_NUMBER32, 0));
    else
      REQUIRE(0 == ham_env_create_db(m_env, &m_db, 1, HAM_RECORD_NUMBER64, 0));

    m_context.reset(new Context((LocalEnvironment *)m_env, 0, 0));
  }

  ~RecordNumberFixture() {
    teardown();
  }

  void teardown() {
    m_context->changeset.clear();
    REQUIRE(0 == ham_env_close(m_env, HAM_AUTO_CLEANUP));
  }

  void reopen() {
    teardown();

    REQUIRE(0 == ham_env_open(&m_env, Utils::opath(".test"), m_flags, 0));
    REQUIRE(0 == ham_env_open_db(m_env, &m_db, 1, 0, 0));
  }

  void createCloseTest() {
    // nop
  }

  void createCloseOpenCloseTest() {
    reopen();

    uint32_t mask = HAM_RECORD_NUMBER32 | HAM_RECORD_NUMBER64;
    REQUIRE((((LocalDatabase *)m_db)->get_flags() & mask) != 0);
  }

  void createInsertCloseReopenTest() {
    ham_key_t key = {};
    ham_record_t rec = {};
    RecnoType recno, value = 1;

    key.flags = HAM_KEY_USER_ALLOC;
    key.data = &recno;
    key.size = sizeof(recno);

    rec.data = &value;
    rec.size = sizeof(value);

    for (int i = 0; i < 5; i++) {
      REQUIRE(0 == ham_db_insert(m_db, 0, &key, &rec, 0));
      REQUIRE(recno == (RecnoType)i + 1);
    }

    key.flags = HAM_KEY_USER_ALLOC;
    key.data = 0;
    REQUIRE(HAM_INV_PARAMETER == ham_db_insert(m_db, 0, &key, &rec, 0));
    key.data = &recno;
    key.size = sizeof(RecnoType) == 4 ? 8 : 4;
    REQUIRE(HAM_INV_KEY_SIZE == ham_db_insert(m_db, 0, &key, &rec, 0));
    key.size = sizeof(recno);

    key.flags = 0;
    key.size = 0;
    REQUIRE(HAM_INV_PARAMETER == ham_db_insert(m_db, 0, &key, &rec, 0));
    key.size = 8;
    key.data = 0;
    REQUIRE(HAM_INV_PARAMETER == ham_db_insert(m_db, 0, &key, &rec, 0));
    key.data = &recno;
    key.size = sizeof(RecnoType);
    key.flags = HAM_KEY_USER_ALLOC;

    reopen();

    for (int i = 5; i < 10; i++) {
      REQUIRE(0 == ham_db_insert(m_db, 0, &key, &rec, 0));
      REQUIRE(recno == (RecnoType)i + 1);
    }
  }

  void createInsertCloseReopenCursorTest(void)
  {
    ham_key_t key = {};
    ham_record_t rec = {};
    ham_cursor_t *cursor;
    RecnoType recno, value = 1;

    key.flags = HAM_KEY_USER_ALLOC;
    key.data = &recno;
    key.size = sizeof(recno);

    rec.data = &value;
    rec.size = sizeof(value);

    REQUIRE(0 == ham_cursor_create(&cursor, m_db, 0, 0));

    for (int i = 0; i < 5; i++) {
      REQUIRE(0 == ham_cursor_insert(cursor, &key, &rec, 0));
      REQUIRE(recno == (RecnoType)i + 1);
    }

    REQUIRE(0 == ham_cursor_close(cursor));
    reopen();
    REQUIRE(0 == ham_cursor_create(&cursor, m_db, 0, 0));

    for (int i = 5; i < 10; i++) {
      REQUIRE(0 == ham_cursor_insert(cursor, &key, &rec, 0));
      REQUIRE(recno == (RecnoType)i + 1);
    }

    REQUIRE(0 == ham_cursor_close(cursor));
  }

  void createInsertCloseTest() {
    ham_key_t key = {};
    ham_record_t rec = {};
    RecnoType recno, value = 1;

    key.flags = HAM_KEY_USER_ALLOC;
    key.data = &recno;
    key.size = sizeof(recno);

    rec.data = &value;
    rec.size = sizeof(value);

    for (int i = 0; i < 5; i++) {
      REQUIRE(0 == ham_db_insert(m_db, 0, &key, &rec, 0));
      REQUIRE(recno == (RecnoType)i + 1);
    }
  }

  void createInsertManyCloseTest() {
    ham_key_t key = {};
    ham_record_t rec = {};
    RecnoType recno, value = 1;

    key.flags = HAM_KEY_USER_ALLOC;
    key.data = &recno;
    key.size = sizeof(recno);

    rec.data = &value;
    rec.size = sizeof(value);

    for (int i = 0; i < 500; i++) {
      REQUIRE(0 == ham_db_insert(m_db, 0, &key, &rec, 0));
      REQUIRE(recno == (RecnoType)i + 1);
    }

    key.size = sizeof(RecnoType) == 4 ? 8 : 4;
    REQUIRE(HAM_INV_KEY_SIZE == ham_db_find(m_db, 0, &key, &rec, 0));
    key.size = 0;
    key.data = &key;
    REQUIRE(HAM_INV_KEY_SIZE == ham_db_find(m_db, 0, &key, &rec, 0));

    for (int i = 0; i < 500; i++) {
      recno = i + 1;
      memset(&key, 0, sizeof(key));
      memset(&rec, 0, sizeof(rec));
      key.data = &recno;
      key.size = sizeof(recno);
      REQUIRE(0 == ham_db_find(m_db, 0, &key, &rec, 0));
    }
  }

  void createInsertCloseCursorTest(void) {
    ham_key_t key = {};
    ham_record_t rec = {};
    ham_cursor_t *cursor;
    RecnoType recno, value = 1;

    key.flags = HAM_KEY_USER_ALLOC;
    key.data = &recno;
    key.size = sizeof(recno);

    rec.data = &value;
    rec.size = sizeof(value);

    REQUIRE(0 == ham_cursor_create(&cursor, m_db, 0, 0));

    for (int i = 0; i < 5; i++) {
      REQUIRE(0 == ham_cursor_insert(cursor, &key, &rec, 0));
      REQUIRE(recno == (RecnoType)i + 1);
    }

    REQUIRE(0 == ham_cursor_close(cursor));
  }

  void createInsertCloseReopenTwiceTest() {
    ham_key_t key = {};
    ham_record_t rec = {};
    RecnoType recno, value = 1;

    key.flags = HAM_KEY_USER_ALLOC;
    key.data = &recno;
    key.size = sizeof(recno);

    rec.data = &value;
    rec.size = sizeof(value);

    for (int i = 0; i < 5; i++) {
      REQUIRE(0 == ham_db_insert(m_db, 0, &key, &rec, 0));
      REQUIRE(recno == (RecnoType)i + 1);
    }

    reopen();

    for (int i = 5; i < 10; i++) {
      REQUIRE(0 == ham_db_insert(m_db, 0, &key, &rec, 0));
      REQUIRE(recno == (RecnoType)i + 1);
    }

    reopen();

    for (int i = 10; i < 15; i++) {
      REQUIRE(0 == ham_db_insert(m_db, 0, &key, &rec, 0));
      REQUIRE(recno == (RecnoType)i + 1);
    }
  }

  void createInsertCloseReopenTwiceCursorTest() {
    ham_key_t key = {};
    ham_record_t rec = {};
    RecnoType recno, value = 1;
    ham_cursor_t *cursor;

    key.flags = HAM_KEY_USER_ALLOC;
    key.data = &recno;
    key.size = sizeof(recno);

    rec.data = &value;
    rec.size = sizeof(value);

    REQUIRE(0 == ham_cursor_create(&cursor, m_db, 0, 0));

    for (int i = 0; i < 5; i++) {
      REQUIRE(0 == ham_cursor_insert(cursor, &key, &rec, 0));
      REQUIRE(recno == (RecnoType)i + 1);
    }

    REQUIRE(0 == ham_cursor_close(cursor));

    reopen();

    REQUIRE(0 == ham_cursor_create(&cursor, m_db, 0, 0));

    for (int i = 5; i < 10; i++) {
      REQUIRE(0 == ham_cursor_insert(cursor, &key, &rec, 0));
      REQUIRE(recno == (RecnoType)i + 1);
    }

    REQUIRE(0 == ham_cursor_close(cursor));
    reopen();
    REQUIRE(0 == ham_cursor_create(&cursor, m_db, 0, 0));

    for (int i = 10; i < 15; i++) {
      REQUIRE(0 == ham_cursor_insert(cursor, &key, &rec, 0));
      REQUIRE(recno == (RecnoType)i + 1);
    }

    REQUIRE(0 == ham_cursor_close(cursor));
  }

  void insertBadKeyTest() {
    ham_key_t key = {};
    ham_record_t rec = {};
    RecnoType recno;

    key.flags = 0;
    key.data = &recno;
    key.size = sizeof(recno);
    REQUIRE(HAM_INV_PARAMETER == ham_db_insert(m_db, 0, &key, &rec, 0));

    key.data = 0;
    key.size = 8;
    REQUIRE(HAM_INV_PARAMETER == ham_db_insert(m_db, 0, &key, &rec, 0));
    REQUIRE(HAM_INV_PARAMETER == ham_db_insert(m_db, 0, 0, &rec, 0));

    key.data = 0;
    key.size = 0;
    REQUIRE(0 == ham_db_insert(m_db, 0, &key, &rec, 0));
    REQUIRE((RecnoType)1ull == *(RecnoType *)key.data);
  }

  void insertBadKeyCursorTest() {
    ham_key_t key = {};
    ham_record_t rec = {};
    ham_cursor_t *cursor;
    RecnoType recno;

    REQUIRE(0 == ham_cursor_create(&cursor, m_db, 0, 0));

    key.flags = 0;
    key.data = &recno;
    key.size = sizeof(recno);
    REQUIRE(HAM_INV_PARAMETER == ham_cursor_insert(cursor, &key, &rec, 0));

    key.data = 0;
    key.size = sizeof(RecnoType);
    REQUIRE(HAM_INV_PARAMETER == ham_cursor_insert(cursor, &key, &rec, 0));

    REQUIRE(HAM_INV_PARAMETER == ham_cursor_insert(cursor, 0, &rec, 0));

    key.data = 0;
    key.size = 0;
    REQUIRE(0 == ham_cursor_insert(cursor, &key, &rec, 0));
    REQUIRE((RecnoType)1ull == *(RecnoType *)key.data);

    REQUIRE(0 == ham_cursor_close(cursor));
  }

  void createBadKeysizeTest() {
    ham_parameter_t p[] = {
      { HAM_PARAM_KEYSIZE, 7 },
      { 0, 0 }
    };

    REQUIRE(HAM_INV_KEYSIZE ==
        ham_env_create_db(m_env, &m_db, 2, HAM_RECORD_NUMBER32, &p[0]));
    REQUIRE(HAM_INV_KEYSIZE ==
        ham_env_create_db(m_env, &m_db, 2, HAM_RECORD_NUMBER64, &p[0]));

    p[0].value = 9;
    REQUIRE(HAM_INV_KEYSIZE ==
        ham_env_create_db(m_env, &m_db, 2, HAM_RECORD_NUMBER32, &p[0]));
    REQUIRE(HAM_INV_KEYSIZE ==
        ham_env_create_db(m_env, &m_db, 3, HAM_RECORD_NUMBER64, &p[0]));
  }

  void envTest() {
    ham_key_t key = {};
    ham_record_t rec = {};
    RecnoType recno;

    key.data = &recno;
    key.size = sizeof(recno);
    key.flags = HAM_KEY_USER_ALLOC;

    teardown();

    REQUIRE(0 ==
        ham_env_create(&m_env, Utils::opath(".test"), m_flags, 0664, 0));
    if (sizeof(RecnoType) == 4)
      REQUIRE(0 ==
          ham_env_create_db(m_env, &m_db, 1, HAM_RECORD_NUMBER32, 0));
    else
      REQUIRE(0 ==
          ham_env_create_db(m_env, &m_db, 1, HAM_RECORD_NUMBER64, 0));

    REQUIRE(0 == ham_db_insert(m_db, 0, &key, &rec, 0));
    REQUIRE((RecnoType)1ull == *(RecnoType *)key.data);

    if (!(m_flags & HAM_IN_MEMORY)) {
      reopen();

      REQUIRE(0 == ham_db_insert(m_db, 0, &key, &rec, 0));
      REQUIRE((RecnoType)2ull == *(RecnoType *)key.data);
    }
  }

  void overwriteTest() {
    ham_key_t key = {};
    ham_record_t rec = {};
    RecnoType recno, value;

    key.data = &recno;
    key.flags = HAM_KEY_USER_ALLOC;
    key.size = sizeof(recno);
    REQUIRE(0 == ham_db_insert(m_db, 0, &key, &rec, 0));

    value = 0x13ull;
    memset(&rec, 0, sizeof(rec));
    rec.data = &value;
    rec.size = sizeof(value);
    REQUIRE(0 == ham_db_insert(m_db, 0, &key, &rec, HAM_OVERWRITE));

    key.size = sizeof(RecnoType) == 4 ? 8 : 4;
    REQUIRE(HAM_INV_KEY_SIZE ==
        ham_db_insert(m_db, 0, &key, &rec, HAM_OVERWRITE));
    key.size = 8;
    key.data = 0;
    REQUIRE(HAM_INV_PARAMETER ==
        ham_db_insert(m_db, 0, &key, &rec, HAM_OVERWRITE));
    key.data = &recno;
    key.size = sizeof(RecnoType);

    memset(&rec, 0, sizeof(rec));
    REQUIRE(0 == ham_db_find(m_db, 0, &key, &rec, 0));

    REQUIRE(value == *(RecnoType *)rec.data);
  }

  void overwriteCursorTest() {
    ham_key_t key = {};
    ham_record_t rec = {};
    RecnoType recno, value;
    ham_cursor_t *cursor;

    REQUIRE(0 == ham_cursor_create(&cursor, m_db, 0, 0));

    key.data = &recno;
    key.flags = HAM_KEY_USER_ALLOC;
    key.size = sizeof(recno);
    REQUIRE(0 == ham_cursor_insert(cursor, &key, &rec, 0));

    value = 0x13ull;
    memset(&rec, 0, sizeof(rec));
    rec.data = &value;
    rec.size = sizeof(value);
    REQUIRE(0 == ham_cursor_insert(cursor, &key, &rec, HAM_OVERWRITE));

    memset(&rec, 0, sizeof(rec));
    REQUIRE(0 == ham_db_find(m_db, 0, &key, &rec, 0));

    REQUIRE(value == *(RecnoType *)rec.data);

    REQUIRE(0 == ham_cursor_close(cursor));
  }

  void eraseLastReopenTest() {
    ham_key_t key = {};
    ham_record_t rec = {};
    RecnoType recno;

    key.data = &recno;
    key.flags = HAM_KEY_USER_ALLOC;
    key.size = sizeof(recno);

    for (int i = 0; i < 5; i++) {
      REQUIRE(0 == ham_db_insert(m_db, 0, &key, &rec, 0));
      REQUIRE(recno == (RecnoType)i + 1);
    }

    REQUIRE(0 == ham_db_erase(m_db, 0, &key, 0));

    reopen();

    for (int i = 5; i < 10; i++) {
      REQUIRE(0 == ham_db_insert(m_db, 0, &key, &rec, 0));
      REQUIRE((RecnoType)i == recno);
    }
  }

  void uncoupleTest() {
    ham_key_t key = {};
    ham_record_t rec = {};
    RecnoType recno;
    ham_cursor_t *cursor, *c2;

    key.flags = HAM_KEY_USER_ALLOC;
    key.data = &recno;
    key.size = sizeof(recno);

    REQUIRE(0 == ham_cursor_create(&cursor, m_db, 0, 0));
    REQUIRE(0 == ham_cursor_create(&c2, m_db, 0, 0));

    for (int i = 0; i < 5; i++) {
      REQUIRE(0 == ham_cursor_insert(cursor, &key, &rec, 0));
      REQUIRE(recno == (RecnoType)i + 1);
    }

    LocalDatabase *db = (LocalDatabase *)m_db;
    BtreeIndex *be = db->btree_index();
    Page *page;
    PageManager *pm = db->lenv()->page_manager();
    REQUIRE((page = pm->fetch(m_context.get(), be->root_address())) != 0);
    REQUIRE(page != 0);
    m_context->changeset.clear(); // unlock pages
    BtreeCursor::uncouple_all_cursors(m_context.get(), page, 0);

    for (int i = 0; i < 5; i++) {
      REQUIRE(0 == ham_cursor_move(c2, &key, &rec, HAM_CURSOR_NEXT));
      REQUIRE(recno == (RecnoType)i + 1);
    }
  }

  void splitTest() {
    ham_key_t key = {};
    ham_record_t rec = {};
    RecnoType recno;

    key.flags = HAM_KEY_USER_ALLOC;
    key.data = &recno;
    key.size = sizeof(recno);

    for (int i = 0; i < 4096; i++) {
      REQUIRE(0 == ham_db_insert(m_db, 0, &key, &rec, 0));
      REQUIRE(recno == (RecnoType)i + 1);
    }
  }

  void overflowTest() {
    ham_key_t key = {};
    ham_record_t rec = {};
    RecnoType recno = std::numeric_limits<RecnoType>::max();
    ((LocalDatabase *)m_db)->m_recno = recno;

    recno = 0;
    key.flags = HAM_KEY_USER_ALLOC;
    key.data = &recno;
    key.size = sizeof(recno);

    REQUIRE(HAM_LIMITS_REACHED == ham_db_insert(m_db, 0, &key, &rec, 0));
  }
};

TEST_CASE("RecordNumber64/createCloseTest", "")
{
  RecordNumberFixture<uint64_t> f;
  f.createCloseTest();
}

TEST_CASE("RecordNumber64/createCloseOpenCloseTest", "")
{
  RecordNumberFixture<uint64_t> f;
  f.createCloseOpenCloseTest();
}

TEST_CASE("RecordNumber64/createInsertCloseTest", "")
{
  RecordNumberFixture<uint64_t> f;
  f.createInsertCloseTest();
}

TEST_CASE("RecordNumber64/createInsertManyCloseTest", "")
{
  RecordNumberFixture<uint64_t> f;
  f.createInsertManyCloseTest();
}

TEST_CASE("RecordNumber64/createInsertCloseCursorTest", "")
{
  RecordNumberFixture<uint64_t> f;
  f.createInsertCloseCursorTest();
}

TEST_CASE("RecordNumber64/createInsertCloseReopenTest", "")
{
  RecordNumberFixture<uint64_t> f;
  f.createInsertCloseReopenTest();
}

TEST_CASE("RecordNumber64/createInsertCloseReopenCursorTest", "")
{
  RecordNumberFixture<uint64_t> f;
  f.createInsertCloseReopenCursorTest();
}

TEST_CASE("RecordNumber64/createInsertCloseReopenTwiceTest", "")
{
  RecordNumberFixture<uint64_t> f;
  f.createInsertCloseReopenTwiceTest();
}

TEST_CASE("RecordNumber64/createInsertCloseReopenTwiceCursorTest", "")
{
  RecordNumberFixture<uint64_t> f;
  f.createInsertCloseReopenTwiceCursorTest();
}

TEST_CASE("RecordNumber64/insertBadKeyTest", "")
{
  RecordNumberFixture<uint64_t> f;
  f.insertBadKeyTest();
}

TEST_CASE("RecordNumber64/insertBadKeyCursorTest", "")
{
  RecordNumberFixture<uint64_t> f;
  f.insertBadKeyCursorTest();
}

TEST_CASE("RecordNumber64/createBadKeysizeTest", "")
{
  RecordNumberFixture<uint64_t> f;
  f.createBadKeysizeTest();
}

TEST_CASE("RecordNumber64/envTest", "")
{
  RecordNumberFixture<uint64_t> f;
  f.envTest();
}

TEST_CASE("RecordNumber64/overwriteTest", "")
{
  RecordNumberFixture<uint64_t> f;
  f.overwriteTest();
}

TEST_CASE("RecordNumber64/overwriteCursorTest", "")
{
  RecordNumberFixture<uint64_t> f;
  f.overwriteCursorTest();
}

TEST_CASE("RecordNumber64/eraseLastReopenTest", "")
{
  RecordNumberFixture<uint64_t> f;
  f.eraseLastReopenTest();
}

TEST_CASE("RecordNumber64/uncoupleTest", "")
{
  RecordNumberFixture<uint64_t> f;
  f.uncoupleTest();
}

TEST_CASE("RecordNumber64/splitTest", "")
{
  RecordNumberFixture<uint64_t> f;
  f.splitTest();
}


TEST_CASE("RecordNumber64-inmem/createCloseTest", "")
{
  RecordNumberFixture<uint64_t> f(HAM_IN_MEMORY);
  f.createCloseTest();
}

TEST_CASE("RecordNumber64-inmem/createInsertCloseTest", "")
{
  RecordNumberFixture<uint64_t> f(HAM_IN_MEMORY);
  f.createInsertCloseTest();
}

TEST_CASE("RecordNumber64-inmem/createInsertManyCloseTest", "")
{
  RecordNumberFixture<uint64_t> f(HAM_IN_MEMORY);
  f.createInsertManyCloseTest();
}

TEST_CASE("RecordNumber64-inmem/createInsertCloseCursorTest", "")
{
  RecordNumberFixture<uint64_t> f(HAM_IN_MEMORY);
  f.createInsertCloseCursorTest();
}

TEST_CASE("RecordNumber64-inmem/insertBadKeyTest", "")
{
  RecordNumberFixture<uint64_t> f(HAM_IN_MEMORY);
  f.insertBadKeyTest();
}

TEST_CASE("RecordNumber64-inmem/insertBadKeyCursorTest", "")
{
  RecordNumberFixture<uint64_t> f(HAM_IN_MEMORY);
  f.insertBadKeyCursorTest();
}

TEST_CASE("RecordNumber64-inmem/createBadKeysizeTest", "")
{
  RecordNumberFixture<uint64_t> f(HAM_IN_MEMORY);
  f.createBadKeysizeTest();
}

TEST_CASE("RecordNumber64-inmem/envTest", "")
{
  RecordNumberFixture<uint64_t> f(HAM_IN_MEMORY);
  f.envTest();
}

TEST_CASE("RecordNumber64-inmem/overwriteTest", "")
{
  RecordNumberFixture<uint64_t> f(HAM_IN_MEMORY);
  f.overwriteTest();
}

TEST_CASE("RecordNumber64-inmem/overwriteCursorTest", "")
{
  RecordNumberFixture<uint64_t> f(HAM_IN_MEMORY);
  f.overwriteCursorTest();
}

TEST_CASE("RecordNumber64-inmem/uncoupleTest", "")
{
  RecordNumberFixture<uint64_t> f(HAM_IN_MEMORY);
  f.uncoupleTest();
}

TEST_CASE("RecordNumber64-inmem/splitTest", "")
{
  RecordNumberFixture<uint64_t> f(HAM_IN_MEMORY);
  f.splitTest();
}

TEST_CASE("RecordNumber32/createCloseTest", "")
{
  RecordNumberFixture<uint32_t> f;
  f.createCloseTest();
}

TEST_CASE("RecordNumber32/createCloseOpenCloseTest", "")
{
  RecordNumberFixture<uint32_t> f;
  f.createCloseOpenCloseTest();
}

TEST_CASE("RecordNumber32/createInsertCloseTest", "")
{
  RecordNumberFixture<uint32_t> f;
  f.createInsertCloseTest();
}

TEST_CASE("RecordNumber32/createInsertManyCloseTest", "")
{
  RecordNumberFixture<uint32_t> f;
  f.createInsertManyCloseTest();
}

TEST_CASE("RecordNumber32/createInsertCloseCursorTest", "")
{
  RecordNumberFixture<uint32_t> f;
  f.createInsertCloseCursorTest();
}

TEST_CASE("RecordNumber32/createInsertCloseReopenTest", "")
{
  RecordNumberFixture<uint32_t> f;
  f.createInsertCloseReopenTest();
}

TEST_CASE("RecordNumber32/createInsertCloseReopenCursorTest", "")
{
  RecordNumberFixture<uint32_t> f;
  f.createInsertCloseReopenCursorTest();
}

TEST_CASE("RecordNumber32/createInsertCloseReopenTwiceTest", "")
{
  RecordNumberFixture<uint32_t> f;
  f.createInsertCloseReopenTwiceTest();
}

TEST_CASE("RecordNumber32/createInsertCloseReopenTwiceCursorTest", "")
{
  RecordNumberFixture<uint32_t> f;
  f.createInsertCloseReopenTwiceCursorTest();
}

TEST_CASE("RecordNumber32/insertBadKeyTest", "")
{
  RecordNumberFixture<uint32_t> f;
  f.insertBadKeyTest();
}

TEST_CASE("RecordNumber32/insertBadKeyCursorTest", "")
{
  RecordNumberFixture<uint32_t> f;
  f.insertBadKeyCursorTest();
}

TEST_CASE("RecordNumber32/createBadKeysizeTest", "")
{
  RecordNumberFixture<uint32_t> f;
  f.createBadKeysizeTest();
}

TEST_CASE("RecordNumber32/envTest", "")
{
  RecordNumberFixture<uint32_t> f;
  f.envTest();
}

TEST_CASE("RecordNumber32/overwriteTest", "")
{
  RecordNumberFixture<uint32_t> f;
  f.overwriteTest();
}

TEST_CASE("RecordNumber32/overwriteCursorTest", "")
{
  RecordNumberFixture<uint32_t> f;
  f.overwriteCursorTest();
}

TEST_CASE("RecordNumber32/eraseLastReopenTest", "")
{
  RecordNumberFixture<uint32_t> f;
  f.eraseLastReopenTest();
}

TEST_CASE("RecordNumber32/uncoupleTest", "")
{
  RecordNumberFixture<uint32_t> f;
  f.uncoupleTest();
}

TEST_CASE("RecordNumber32/splitTest", "")
{
  RecordNumberFixture<uint32_t> f;
  f.splitTest();
}

TEST_CASE("RecordNumber32-inmem/createCloseTest", "")
{
  RecordNumberFixture<uint32_t> f(HAM_IN_MEMORY);
  f.createCloseTest();
}

TEST_CASE("RecordNumber32-inmem/createInsertCloseTest", "")
{
  RecordNumberFixture<uint32_t> f(HAM_IN_MEMORY);
  f.createInsertCloseTest();
}

TEST_CASE("RecordNumber32-inmem/createInsertManyCloseTest", "")
{
  RecordNumberFixture<uint32_t> f(HAM_IN_MEMORY);
  f.createInsertManyCloseTest();
}

TEST_CASE("RecordNumber32-inmem/createInsertCloseCursorTest", "")
{
  RecordNumberFixture<uint32_t> f(HAM_IN_MEMORY);
  f.createInsertCloseCursorTest();
}

TEST_CASE("RecordNumber32-inmem/insertBadKeyTest", "")
{
  RecordNumberFixture<uint32_t> f(HAM_IN_MEMORY);
  f.insertBadKeyTest();
}

TEST_CASE("RecordNumber32-inmem/insertBadKeyCursorTest", "")
{
  RecordNumberFixture<uint32_t> f(HAM_IN_MEMORY);
  f.insertBadKeyCursorTest();
}

TEST_CASE("RecordNumber32-inmem/createBadKeysizeTest", "")
{
  RecordNumberFixture<uint32_t> f(HAM_IN_MEMORY);
  f.createBadKeysizeTest();
}

TEST_CASE("RecordNumber32-inmem/envTest", "")
{
  RecordNumberFixture<uint32_t> f(HAM_IN_MEMORY);
  f.envTest();
}

TEST_CASE("RecordNumber32-inmem/overwriteTest", "")
{
  RecordNumberFixture<uint32_t> f(HAM_IN_MEMORY);
  f.overwriteTest();
}

TEST_CASE("RecordNumber32-inmem/overwriteCursorTest", "")
{
  RecordNumberFixture<uint32_t> f(HAM_IN_MEMORY);
  f.overwriteCursorTest();
}

TEST_CASE("RecordNumber32-inmem/uncoupleTest", "")
{
  RecordNumberFixture<uint32_t> f(HAM_IN_MEMORY);
  f.uncoupleTest();
}

TEST_CASE("RecordNumber32-inmem/splitTest", "")
{
  RecordNumberFixture<uint32_t> f(HAM_IN_MEMORY);
  f.splitTest();
}

TEST_CASE("RecordNumber64/overflowTest", "")
{
  RecordNumberFixture<uint64_t> f;
  f.overflowTest();
}

TEST_CASE("RecordNumber32/overflowTest", "")
{
  RecordNumberFixture<uint32_t> f;
  f.overflowTest();
}

} // namespace hamsterdb
