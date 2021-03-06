#include <ssftbl.h>

#include <map>
#include <vector>
#include <string>
#include <gtest/gtest.h>

using namespace std;

namespace {
string get_random_str(int minlen, int maxlen) {
  string s;
  int len = minlen + rand() % (maxlen - minlen);
  for (int i = 0; i < len; i++)
    s += 'a' + rand() % 26;
  return s;
}
}

/*-----------------------------------------------------------------------------
 * Open and Close
 */
class SSFTBLTestFixture : public testing::Test {
protected:
  void SetUp() {
    ftbl = ssftblnew();
    ASSERT_TRUE(ftbl != NULL);
  }
  void TearDown() {
    ssftbldel(ftbl);
  }
  SSFTBL *ftbl;
};

TEST_F(SSFTBLTestFixture, new_del) {
  // nothing
  ;
}

/*-----------------------------------------------------------------------------
 * Writer
 */
class SSFTBLWriterTestFixture : public SSFTBLTestFixture {
protected:
  void SetUp() {
    dbname = "./ssftblwritertest";
    unlink((dbname + ".sstbl").c_str());

    SSFTBLTestFixture::SetUp();
    int r;
    r = ssftblopen(ftbl, dbname.c_str(), SSFTBLOWRITER);
    ASSERT_EQ(0, r);
  }
  void TearDown() {
    int r;
    r = ssftblclose(ftbl);
    ASSERT_EQ(0, r);

    ASSERT_EQ(0, unlink((dbname + ".sstbl").c_str()));

    SSFTBLTestFixture::TearDown();
  }
  string dbname;
};

TEST_F(SSFTBLWriterTestFixture, open_close) {
  // nothing
  ;
}

TEST_F(SSFTBLWriterTestFixture, append_1) {
  int r;
  string key = "key";
  string val = "val";
  r = ssftblappend(ftbl, key.c_str(), key.size(), val.c_str(), val.size());
  ASSERT_EQ(0, r);
}

TEST_F(SSFTBLWriterTestFixture, append_2_asc_order) {
  int r;
  string key, val;
  key = "key1";
  val = "val1";
  r = ssftblappend(ftbl, key.c_str(), key.size(), val.c_str(), val.size());
  ASSERT_EQ(0, r);

  ASSERT_EQ(key.size(), ftbl->lastappended.ksiz);
  EXPECT_EQ(0, memcmp(key.c_str(), ftbl->lastappended.kbuf, key.size()));

  key = "key2";
  val = "val2";
  r = ssftblappend(ftbl, key.c_str(), key.size(), val.c_str(), val.size());
  ASSERT_EQ(0, r);
}

TEST_F(SSFTBLWriterTestFixture, append_2_desc_order) {
  int r;
  string key, val;
  key = "key2";
  val = "val2";
  r = ssftblappend(ftbl, key.c_str(), key.size(), val.c_str(), val.size());
  ASSERT_EQ(0, r);

  ASSERT_EQ(key.size(), ftbl->lastappended.ksiz);
  EXPECT_EQ(0, memcmp(key.c_str(), ftbl->lastappended.kbuf, key.size()));

  key = "key1";
  val = "val1";
  r = ssftblappend(ftbl, key.c_str(), key.size(), val.c_str(), val.size());
  ASSERT_EQ(-1, r);
}

/*-----------------------------------------------------------------------------
 * SimpleReader
 */
class SSFTBLBaseReaderTestFixture : public SSFTBLTestFixture {
protected:
  void SetUp() {
    dbname = "./ssftblreadertest";
    unlink((dbname + ".sstbl").c_str());

    SSFTBLTestFixture::SetUp();
    int r;
    int cmode = SSFTBLCMETHOD;
    r = ssftbltune(ftbl, 64 * 1024, cmode);
    ASSERT_EQ(0, r);

    r = ssftblopen(ftbl, dbname.c_str(), SSFTBLOWRITER);
    ASSERT_EQ(0, r);

    Appends(ftbl);

    r = ssftblclose(ftbl);
    ASSERT_EQ(0, r);

    r = ssftblopen(ftbl, dbname.c_str(), SSFTBLOREADER);
    ASSERT_EQ(0, r);
  }
  void TearDown() {
    int r;
    r = ssftblclose(ftbl);
    ASSERT_EQ(0, r);

    ASSERT_EQ(0, unlink((dbname + ".sstbl").c_str()));

    SSFTBLTestFixture::TearDown();
  }
  virtual void Appends(SSFTBL *ftbl) = 0;
  string dbname;
};

class SSFTBLReaderTestFixture : public SSFTBLBaseReaderTestFixture {
public:
  virtual void Appends(SSFTBL *ftbl) {
    int r;
    string key, val;
    key = "key1";
    val = "val1";
    r = ssftblappend(ftbl, key.c_str(), key.size(), val.c_str(), val.size());
    ASSERT_EQ(0, r);

    ASSERT_EQ(key.size(), ftbl->lastappended.ksiz);
    EXPECT_EQ(0, memcmp(key.c_str(), ftbl->lastappended.kbuf, key.size()));

    key = "key3";
    val = "val3";
    r = ssftblappend(ftbl, key.c_str(), key.size(), val.c_str(), val.size());
    ASSERT_EQ(0, r);
  }
};

TEST_F(SSFTBLReaderTestFixture, open_close) {
  // nothing
  ;
}

TEST_F(SSFTBLReaderTestFixture, get1) {
  void *p;
  int sp;
  string key;

  // out-of-range in index
  key = "key0";
  p = ssftblget(ftbl, key.c_str(), key.size(), &sp);
  ASSERT_EQ(NULL, p);
  key = "key4";
  p = ssftblget(ftbl, key.c_str(), key.size(), &sp);
  ASSERT_EQ(NULL, p);

  // in-range in index
  key = "key1";
  p = ssftblget(ftbl, key.c_str(), key.size(), &sp);
  ASSERT_TRUE(p != NULL);
  ASSERT_EQ("val1", string((const char*)p, sp));
  key = "key2";
  p = ssftblget(ftbl, key.c_str(), key.size(), &sp);
  ASSERT_TRUE(p == NULL);
  key = "key3";
  p = ssftblget(ftbl, key.c_str(), key.size(), &sp);
  ASSERT_TRUE(p != NULL);
  ASSERT_EQ("val3", string((const char*)p, sp));
}

/*-----------------------------------------------------------------------------
 * RandomReader
 */
class SSFTBLRandomReaderTestFixture : public SSFTBLBaseReaderTestFixture {
public:
  virtual void Appends(SSFTBL *ftbl) {
    int r;
    for (int i = 0; i < 100; i++) {
      string key = get_random_str(10, 20);
      if (m.find(key) != m.end()) {
        i--;
        continue;
      }
      string val = get_random_str(1024 * 1, 1024 * 2);
      m[key] = val;
    }
    for (map<string, string>::const_iterator it = m.begin(); it != m.end(); ++it) {
      const string &key = it->first;
      const string &val = it->second;
      r = ssftblappend(ftbl, key.c_str(), key.size(), val.c_str(), val.size());
      ASSERT_EQ(0, r);
    }
  }
  map<string, string> m;
};

TEST_F(SSFTBLRandomReaderTestFixture, open_close) {
  // nothing
  ;
}

TEST_F(SSFTBLRandomReaderTestFixture, get_many) {
  for (map<string, string>::const_iterator it = m.begin(); it != m.end(); ++it) {
    int sp;
    const string &key = it->first;
    const string &val = it->second;
    void *p = ssftblget(ftbl, key.c_str(), key.size(), &sp);
    if (p == NULL)
      cerr << "errkey:" << key << endl;
    ASSERT_TRUE(p != NULL);
    ASSERT_EQ(val, string((const char*)p, sp));
    free(p);
  }
}

TEST_F(SSFTBLRandomReaderTestFixture, get_many_include_not_found) {
  for (unsigned int i = 0; i < 10240; i++) {
    int sp;
    string key = get_random_str(10, 20);
    bool has = (m.find(key) != m.end());
    void *p = ssftblget(ftbl, key.c_str(), key.size(), &sp);
    if (has) {
      if (p == NULL)
        cerr << "errkey:" << key << endl;
      ASSERT_TRUE(p != NULL);
      free(p);
    } else {
      ASSERT_TRUE(p == NULL);
    }
  }
}

/*-----------------------------------------------------------------------------
 * SmallBlockReader
 */
class SSFTBLSmallBlockReaderTestFixture : public SSFTBLBaseReaderTestFixture {
public:
  virtual void Appends(SSFTBL *ftbl) {
    int r;
    for (int i = 0; i < 100; i++) {
      string key = get_random_str(10, 20);
      if (m.find(key) != m.end()) {
        i--;
        continue;
      }
      string val = get_random_str(1024 * 1, 1024 * 2);
      m[key] = val;
    }
    for (map<string, string>::const_iterator it = m.begin(); it != m.end(); ++it) {
      const string &key = it->first;
      const string &val = it->second;
      r = ssftblappend(ftbl, key.c_str(), key.size(), val.c_str(), val.size());
      ASSERT_EQ(0, r);
    }
  }
  map<string, string> m;
};

TEST_F(SSFTBLSmallBlockReaderTestFixture, open_close) {
  // nothing
  ;
}

TEST_F(SSFTBLSmallBlockReaderTestFixture, get_many) {
  for (map<string, string>::const_iterator it = m.begin(); it != m.end(); ++it) {
    int sp;
    const string &key = it->first;
    const string &val = it->second;
    void *p = ssftblget(ftbl, key.c_str(), key.size(), &sp);
    if (p == NULL)
      cerr << "errkey:" << key << endl;
    ASSERT_TRUE(p != NULL);
    ASSERT_EQ(val, string((const char*)p, sp));
    free(p);
  }
}

TEST_F(SSFTBLSmallBlockReaderTestFixture, get_many_include_not_found) {
  for (unsigned int i = 0; i < 10240; i++) {
    int sp;
    string key = get_random_str(10, 20);
    bool has = (m.find(key) != m.end());
    void *p = ssftblget(ftbl, key.c_str(), key.size(), &sp);
    if (has) {
      if (p == NULL)
        cerr << "errkey:" << key << endl;
      ASSERT_TRUE(p != NULL);
      free(p);
    } else {
      ASSERT_TRUE(p == NULL);
    }
  }
}

/*-----------------------------------------------------------------------------
 * LargeBlockReader
 */
class SSFTBLLargeBlockReaderTestFixture : public SSFTBLBaseReaderTestFixture {
public:
  virtual void Appends(SSFTBL *ftbl) {
    int r;
    for (int i = 0; i < 100; i++) {
      string key = get_random_str(10, 20);
      if (m.find(key) != m.end()) {
        i--;
        continue;
      }
      string val = get_random_str(1024 * 100, 1024 * 101);
      m[key] = val;
    }
    for (map<string, string>::const_iterator it = m.begin(); it != m.end(); ++it) {
      const string &key = it->first;
      const string &val = it->second;
      r = ssftblappend(ftbl, key.c_str(), key.size(), val.c_str(), val.size());
      ASSERT_EQ(0, r);
    }
  }
  map<string, string> m;
};

TEST_F(SSFTBLLargeBlockReaderTestFixture, open_close) {
  // nothing
  ;
}

TEST_F(SSFTBLLargeBlockReaderTestFixture, get_many) {
  for (map<string, string>::const_iterator it = m.begin(); it != m.end(); ++it) {
    int sp;
    const string &key = it->first;
    const string &val = it->second;
    void *p = ssftblget(ftbl, key.c_str(), key.size(), &sp);
    if (p == NULL)
      cerr << "errkey:" << key << endl;
    ASSERT_TRUE(p != NULL);
    ASSERT_EQ(val, string((const char*)p, sp));
    free(p);
  }
}

TEST_F(SSFTBLLargeBlockReaderTestFixture, get_many_include_not_found) {
  for (unsigned int i = 0; i < 10240; i++) {
    int sp;
    string key = get_random_str(10, 20);
    bool has = (m.find(key) != m.end());
    void *p = ssftblget(ftbl, key.c_str(), key.size(), &sp);
    if (has) {
      if (p == NULL)
        cerr << "errkey:" << key << endl;
      ASSERT_TRUE(p != NULL);
      free(p);
    } else {
      ASSERT_TRUE(p == NULL);
    }
  }
}
