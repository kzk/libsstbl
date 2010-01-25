#include <ssftbl.h>

#include <vector>
#include <string>
#include <gtest/gtest.h>

using namespace std;

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

class SSFTBLWriterTestFixture : public SSFTBLTestFixture {
protected:
  void SetUp() {
    dbname = "./ssftblwritertest";
    unlink((dbname + ".sstbld").c_str());
    unlink((dbname + ".sstbli").c_str());

    SSFTBLTestFixture::SetUp();
    int r;
    r = ssftblopen(ftbl, dbname.c_str(), SSFTBLOWRITER);
    ASSERT_EQ(0, r);
  }
  void TearDown() {
    int r;
    r = ssftblclose(ftbl);
    ASSERT_EQ(0, r);

    ASSERT_EQ(0, unlink((dbname + ".sstbld").c_str()));
    ASSERT_EQ(0, unlink((dbname + ".sstbli").c_str()));

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

class SSFTBLReaderTestFixture : public SSFTBLTestFixture {
protected:
  void SetUp() {
    dbname = "./ssftblreadertest";
    unlink((dbname + ".sstbld").c_str());
    unlink((dbname + ".sstbli").c_str());

    SSFTBLTestFixture::SetUp();
    int r;
    r = ssftblopen(ftbl, dbname.c_str(), SSFTBLOWRITER);
    ASSERT_EQ(0, r);

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

    r = ssftblclose(ftbl);
    ASSERT_EQ(0, r);

    r = ssftblopen(ftbl, dbname.c_str(), SSFTBLOREADER);
    ASSERT_EQ(0, r);
  }
  void TearDown() {
    int r;
    r = ssftblclose(ftbl);
    ASSERT_EQ(0, r);

    ASSERT_EQ(0, unlink((dbname + ".sstbld").c_str()));
    ASSERT_EQ(0, unlink((dbname + ".sstbli").c_str()));

    SSFTBLTestFixture::TearDown();
  }
  string dbname;
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
  key = "key2";
  p = ssftblget(ftbl, key.c_str(), key.size(), &sp);
  ASSERT_TRUE(p != NULL);
  key = "key3";
  p = ssftblget(ftbl, key.c_str(), key.size(), &sp);
  ASSERT_TRUE(p != NULL);

}
