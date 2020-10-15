#include "gtest/gtest.h"

#include <parlay/hash_table.h>

TEST(TestHashtable, TestConstruction) {
  parlay::hashtable<parlay::hash_numeric<int>>
    table(400000, parlay::hash_numeric<int>{});;
  

}

TEST(TestHashtable, TestInsert) {
  parlay::hashtable<parlay::hash_numeric<int>>
    table(400000, parlay::hash_numeric<int>{});;
  
  parlay::parallel_for(1, 100000, [&](int i) {
    table.insert(i);
  });
}

TEST(TestHashtable, TestFind) {
  parlay::hashtable<parlay::hash_numeric<int>>
    table(400000, parlay::hash_numeric<int>{});;
  
  parlay::parallel_for(1, 100000, [&](int i) {
    table.insert(i);
  });
  
  parlay::parallel_for(1, 100000, [&](int i) {
    auto val = table.find(i);
    ASSERT_EQ(val, i);
  });
}
  
TEST(TestHashtable, TestDelete) {
  parlay::hashtable<parlay::hash_numeric<int>>
    table(400000, parlay::hash_numeric<int>{});;
  
  parlay::parallel_for(1, 100000, [&](int i) {
    table.insert(i);
  });
  
  parlay::parallel_for(1, 100000, [&](int i) {
    auto val = table.find(i);
    ASSERT_EQ(val, i);
  });
  
  parlay::parallel_for(1, 100000, [&](int i) {
    if (i % 2 == 0) {
      table.deleteVal(i);
    }
  });
  
  parlay::parallel_for(1, 100000, [&](int i) {
    auto val = table.find(i);
    if (i % 2 == 1) {
      ASSERT_EQ(val, i);
    }
    else {
      ASSERT_EQ(val, -1);
    }
  });
}