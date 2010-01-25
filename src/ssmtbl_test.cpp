#include <ssmtbl.h>

#include <vector>
#include <string>
#include <gtest/gtest.h>

using namespace std;

class SSMTBLTestFixture : public testing::Test {
protected:
  void SetUp() {
    mtbl = ssmtblnew();
    ASSERT_TRUE(mtbl != NULL);
    EXPECT_EQ(0, ssmtblrnum(mtbl));
    EXPECT_EQ(0, ssmtblmsiz(mtbl));
  }
  void TearDown() {
    ssmtbldel(mtbl);
  }
  SSMTBL *mtbl;
};

TEST_F(SSMTBLTestFixture, new_del) {
  // nothing
  ;
}

TEST_F(SSMTBLTestFixture, put_get_1) {
  int r;
  r = ssmtblput(mtbl, "key", 3, "val", 3);
  ASSERT_EQ(r, 0);
  EXPECT_EQ(1, ssmtblrnum(mtbl));
  EXPECT_TRUE(0 < ssmtblmsiz(mtbl));

  int sp;
  void *p;
  p = ssmtblget(mtbl, "key", 3, &sp);
  ASSERT_TRUE(p != NULL);
  EXPECT_EQ(sp, 3);
  EXPECT_EQ(string((const char*)p), string("val"));

  p = ssmtblget(mtbl, "key_not_found", 13, &sp);
  ASSERT_TRUE(p == NULL);
}

TEST_F(SSMTBLTestFixture, put_overwrite)
{
  int r;
  int sp;
  void *p;
  {
    r = ssmtblput(mtbl, "key", 3, "val1", 4);
    ASSERT_EQ(r, 0);
    EXPECT_EQ(1, ssmtblrnum(mtbl));
    EXPECT_TRUE(0 < ssmtblmsiz(mtbl));

    p = ssmtblget(mtbl, "key", 3, &sp);
    ASSERT_TRUE(p != NULL);
    EXPECT_EQ(sp, 4);
    EXPECT_EQ(string((const char*)p), string("val1"));
  }
  {
    r = ssmtblput(mtbl, "key", 3, "val2", 4);
    ASSERT_EQ(r, 0);
    EXPECT_EQ(1, ssmtblrnum(mtbl));
    EXPECT_TRUE(0 < ssmtblmsiz(mtbl));

    p = ssmtblget(mtbl, "key", 3, &sp);
    ASSERT_TRUE(p != NULL);
    EXPECT_EQ(sp, 4);
    EXPECT_EQ(string((const char*)p), string("val2"));
  }
}

TEST_F(SSMTBLTestFixture, put_get_1000) {
  vector<string> vs;
  for (int i = 0; i < 1000; i++) {
    string s;
    do {
      for (int i = 0; i < rand()%1024; i++)
        s += 'a' + rand() % 26;
    } while (count(vs.begin(), vs.end(), s) > 0);
    vs.push_back(s);
  }
  for (unsigned int i = 0; i < vs.size(); i++) {
    const string& s = vs[i];
    ASSERT_EQ(0, ssmtblput(mtbl, s.c_str(), s.size(), s.c_str(), s.size()));
    EXPECT_EQ(i+1, ssmtblrnum(mtbl));
  }
  for (unsigned int i = 0; i < vs.size(); i++) {
    int sp;
    const string& s = vs[i];
    void *p = ssmtblget(mtbl, s.c_str(), s.size(), &sp);
    ASSERT_TRUE(p != NULL);
    EXPECT_EQ(string((const char*)p), s);
  }
}
