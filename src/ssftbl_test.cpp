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
  void TearDow() {
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
    SSFTBLTestFixture::SetUp();
    int r;
    r = ssftblopen(ftbl, "./ssftblwritertest", SSFTBLOWRITER);
    ASSERT_EQ(0, r);
  }
  void TearDow() {
    SSFTBLTestFixture::TearDown();
    int r;
    r = ssftblclose(ftbl);
    ASSERT_EQ(0, r);
  }
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

  ASSERT_EQ(key.size(), ftbl->lastappendedkeysiz);
  EXPECT_EQ(0, memcmp(key.c_str(), ftbl->lastappendedkey, key.size()));

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

  ASSERT_EQ(key.size(), ftbl->lastappendedkeysiz);
  EXPECT_EQ(0, memcmp(key.c_str(), ftbl->lastappendedkey, key.size()));

  key = "key1";
  val = "val1";
  r = ssftblappend(ftbl, key.c_str(), key.size(), val.c_str(), val.size());
  ASSERT_EQ(-1, r);
}
