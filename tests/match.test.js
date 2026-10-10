// tests/match.test.js — Node unit test suite for EchoBand keyword matcher
import { matchAnnouncement, extractPlatformNumber, ALERT_TYPES } from '../phone/match.js';

let totalTests = 0;
let passedTests = 0;

function assert(condition, message) {
  totalTests++;
  if (condition) {
    passedTests++;
    console.log(`  ✓ ${message}`);
  } else {
    console.error(`  ✗ FAIL: ${message}`);
  }
}

console.log('--- Running EchoBand Matcher Unit Tests ---');

console.log('\n1. English Announcements:');
{
  const r1 = matchAnnouncement("The train to Ernakulam will depart from platform two");
  assert(r1.matched && r1.type === ALERT_TYPES.PLATFORM && r1.arg === 2,
    "Matches Platform 2 (Priority: PLATFORM over DEPARTURE)");

  const r2 = matchAnnouncement("Train 12625 is delayed by fifteen minutes");
  assert(r2.matched && r2.type === ALERT_TYPES.DELAY,
    "Matches Delay from 'delayed'");

  const r3 = matchAnnouncement("Service cancelled due to technical difficulties");
  assert(r3.matched && r3.type === ALERT_TYPES.CANCELLED,
    "Matches Cancelled from 'cancelled'");

  const r4 = matchAnnouncement("Urgent announcement: please evacuate immediately");
  assert(r4.matched && r4.type === ALERT_TYPES.URGENT,
    "Matches Urgent from 'urgent'");

  const r5 = matchAnnouncement("Emergency security alert in main concourse");
  assert(r5.matched && r5.type === ALERT_TYPES.URGENT,
    "Matches Urgent from 'emergency'");

  const r6 = matchAnnouncement("The express train is departing shortly");
  assert(r6.matched && r6.type === ALERT_TYPES.DEPARTURE,
    "Matches Departure from 'departing'");
}

console.log('\n2. Malayalam Announcements:');
{
  const r1 = matchAnnouncement("അടിയന്തര അറിയിപ്പ്: ശ്രദ്ധിക്കുക");
  assert(r1.matched && r1.type === ALERT_TYPES.URGENT,
    "Matches Malayalam Urgent (അടിയന്തര)");

  const r2 = matchAnnouncement("ട്രെയിൻ റദ്ദാക്കി");
  assert(r2.matched && r2.type === ALERT_TYPES.CANCELLED,
    "Matches Malayalam Cancelled (റദ്ദാക്കി)");

  const r3 = matchAnnouncement("ട്രെയിൻ അര മണിക്കൂർ വൈകുന്നു");
  assert(r3.matched && r3.type === ALERT_TYPES.DELAY,
    "Matches Malayalam Delay (വൈകുന്നു)");

  const r4 = matchAnnouncement("ട്രെയിൻ പ്ലാറ്റ്ഫോം മൂന്ന് ൽ എത്തും");
  assert(r4.matched && r4.type === ALERT_TYPES.PLATFORM && r4.arg === 3,
    "Matches Malayalam Platform 3 (പ്ലാറ്റ്ഫോം മൂന്ന്)");

  const r5 = matchAnnouncement("വണ്ടി ഉടൻ പുറപ്പെടുന്നു");
  assert(r5.matched && r5.type === ALERT_TYPES.DEPARTURE,
    "Matches Malayalam Departure (പുറപ്പെടുന്നു)");
}

console.log('\n3. Hindi Announcements:');
{
  const r1 = matchAnnouncement("तत्काल सूचना: कृपया ध्यान दें");
  assert(r1.matched && r1.type === ALERT_TYPES.URGENT,
    "Matches Hindi Urgent (तत्काल)");

  const r2 = matchAnnouncement("ट्रेन रद्द है");
  assert(r2.matched && r2.type === ALERT_TYPES.CANCELLED,
    "Matches Hindi Cancelled (रद्द)");

  const r3 = matchAnnouncement("गाड़ी संख्या 12001 देरी से चल रही है");
  assert(r3.matched && r3.type === ALERT_TYPES.DELAY,
    "Matches Hindi Delay (देरी)");

  const r4 = matchAnnouncement("गाड़ी प्लेटफ़ॉर्म एक पर आएगी");
  assert(r4.matched && r4.type === ALERT_TYPES.PLATFORM && r4.arg === 1,
    "Matches Hindi Platform 1 (प्लेटफ़ॉर्म एक)");

  const r5 = matchAnnouncement("ट्रेन 12626 रवाना हो रही है");
  assert(r5.matched && r5.type === ALERT_TYPES.DEPARTURE,
    "Matches Hindi Departure (रवाना)");
}

console.log('\n4. Priority Resolution:');
{
  // Urgent + Cancelled + Platform: URGENT must win
  const r1 = matchAnnouncement("Urgent: platform 3 train is cancelled");
  assert(r1.matched && r1.type === ALERT_TYPES.URGENT,
    "Urgent takes precedence over Cancelled and Platform");

  // Cancelled + Delay: CANCELLED must win
  const r2 = matchAnnouncement("Delayed service has now been cancelled");
  assert(r2.matched && r2.type === ALERT_TYPES.CANCELLED,
    "Cancelled takes precedence over Delay");
}

console.log('\n5. Platform Number Extraction:');
{
  assert(extractPlatformNumber("Platform 1") === 1, "Extracts digit 1");
  assert(extractPlatformNumber("platform 5") === 5, "Extracts digit 5");
  assert(extractPlatformNumber("platform four") === 4, "Extracts English word 'four'");
  assert(extractPlatformNumber("പ്ലാറ്റ്ഫോം രണ്ട്") === 2, "Extracts Malayalam word 'രണ്ട്' (2)");
  assert(extractPlatformNumber("प्लेटफ़ॉर्म तीन") === 3, "Extracts Hindi word 'तीन' (3)");
  assert(extractPlatformNumber("General announcement with no platform") === 0, "Returns 0 when no number");
}

console.log('\n6. Negative Cases (No alert recognized):');
{
  const n1 = matchAnnouncement("Good morning passengers, have a safe and pleasant journey");
  assert(!n1.matched && n1.type === ALERT_TYPES.NONE, "Ignores pleasant greetings");

  const n2 = matchAnnouncement("Please take care of your personal belongings and luggage");
  assert(!n2.matched && n2.type === ALERT_TYPES.NONE, "Ignores general luggage warning");

  const n3 = matchAnnouncement("");
  assert(!n3.matched && n3.type === ALERT_TYPES.NONE, "Handles empty string gracefully");

  const n4 = matchAnnouncement(null);
  assert(!n4.matched && n4.type === ALERT_TYPES.NONE, "Handles null input safely");
}

console.log(`\n========================================`);
console.log(`Test Results: ${passedTests} / ${totalTests} Passed`);
console.log(`========================================`);

if (passedTests !== totalTests) {
  process.exit(1);
}
