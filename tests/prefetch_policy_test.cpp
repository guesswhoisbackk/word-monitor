#include "study_plan.hpp"
#include "cache_policy.hpp"
#include "network_policy.hpp"
#include <cassert>
#include <cstdio>
using namespace wordmon;
int main() {
  const uint32_t now = 1800000000;
  StudyPlan plan;
  for (uint16_t i=0; i<8; ++i) {
    ReviewRecord record; record.due=now-100+i;
    plan.consider(20-i,&record,now);
  }
  plan.consider(0,nullptr,now);
  assert(plan.dueCount==8 && plan.best(false)==20);
  assert(plan.prefetch(4)==16 && plan.prefetch(5)==StudyPlan::none);
  StudyPlan fresh;
  fresh.consider(3,nullptr,now); fresh.consider(7,nullptr,now);
  ReviewRecord future; future.due=now+10; fresh.consider(1,&future,now);
  assert(fresh.best(false)==-1 && fresh.best(true)==3);
  assert(fresh.prefetch(1)==7 && fresh.nextDue==now+10);
  AssetRetries retries;
  const auto key=assetKey("https://one/picture");
  assert(key!=assetKey("https://two/picture"));
  retries.failed(key,0); assert(!retries.ready(key,29999) && retries.ready(key,30000));
  retries.failed(key,30000); assert(!retries.ready(key,149999) && retries.ready(key,150000));
  retries.failed(key,150000); assert(!retries.ready(key,749999) && retries.ready(key,750000));
  retries.succeeded(key); assert(retries.ready(key,0));
  retries.failed(key,UINT32_MAX-100); assert(!retries.ready(key,100) && retries.ready(key,30000));
  SetupRecovery setup;
  assert(setup.poll(0,false,true,0)==SetupAction::None);
  assert(setup.poll(59999,false,true,0)==SetupAction::None);
  assert(setup.poll(60000,false,true,0)==SetupAction::Retry);
  assert(setup.poll(61000,false,true,1)==SetupAction::None);
  assert(setup.poll(120000,false,true,0)==SetupAction::None);
  assert(setup.poll(180000,false,true,0)==SetupAction::Retry);
  assert(setup.poll(181000,true,true,0)==SetupAction::None);
  assert(setup.poll(190999,true,true,0)==SetupAction::None);
  assert(setup.poll(191000,true,true,0)==SetupAction::Finish);
  setup.reset(); setup.poll(0,true,true,1);
  assert(setup.poll(60000,true,true,1)==SetupAction::None);
  assert(setup.poll(60001,true,true,0)==SetupAction::None);
  assert(setup.poll(70001,true,true,0)==SetupAction::Finish);
  setup.reset(); setup.poll(0,false,false,0);
  assert(setup.poll(999999,false,false,0)==SetupAction::None);
  setup.reset(); setup.poll(UINT32_MAX-100,false,true,0);
  assert(setup.poll(60000,false,true,0)==SetupAction::Retry);
  puts("prefetch, retry and setup recovery policies passed");
}
