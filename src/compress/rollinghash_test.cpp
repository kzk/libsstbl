#include <compress/rollinghash.h>

#include <gtest/gtest.h>

using namespace std;

class RollingHashTestFixture : public testing::Test {
protected:
  void SetUp() {
    wsiz = 3;
    h = rollinghashnew(wsiz);
    ASSERT_TRUE(h != NULL);
  }
  void TearDown() {
    rollinghashdel(h);
  }
  size_t wsiz;
  ROLLINGHASH *h;
};

TEST_F(RollingHashTestFixture, none) {
}

TEST_F(RollingHashTestFixture, simple) {
  int numblk = 10000;
  string s;
  for (size_t i = 0; i < wsiz * numblk; i++)
    s += 'a' + rand() % 26;
  uint32_t rhash = rollinghashdohash(h, s.c_str());
  for (size_t i = 0; i < wsiz * (numblk-1); i++) {
    uint32_t curhash = rollinghashdohash(h, s.c_str() + i);
    EXPECT_EQ(rhash, curhash);
    rhash = rollinghashupdate(h, rhash, s.c_str()[i], s.c_str()[i+wsiz]);
  }
}
