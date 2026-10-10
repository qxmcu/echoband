// match.js — Pure function keyword matcher for EchoBand
// Handles English, Malayalam, and Hindi speech transcripts

export const ALERT_TYPES = {
  NONE: 0,
  PLATFORM: 1,
  DELAY: 2,
  DEPARTURE: 3,
  URGENT: 4,
  CANCELLED: 5
};

export const ALERT_NAMES = {
  [ALERT_TYPES.PLATFORM]: 'PLATFORM',
  [ALERT_TYPES.DELAY]: 'DELAY',
  [ALERT_TYPES.DEPARTURE]: 'DEPARTURE',
  [ALERT_TYPES.URGENT]: 'URGENT',
  [ALERT_TYPES.CANCELLED]: 'CANCELLED'
};

export const ALERT_LABELS = {
  [ALERT_TYPES.PLATFORM]: {
    en: (n) => n ? `Platform ${n}` : 'Platform',
    ml: (n) => n ? `പ്ലാറ്റ്ഫോം ${n}` : 'പ്ലാറ്റ്ഫോം',
    hi: (n) => n ? `प्लेटफ़ॉर्म ${n}` : 'प्लेटफ़ॉर्म'
  },
  [ALERT_TYPES.DELAY]: {
    en: () => 'Train delayed',
    ml: () => 'ട്രെയിൻ വൈകുന്നു',
    hi: () => 'ट्रेन देरी से है'
  },
  [ALERT_TYPES.DEPARTURE]: {
    en: () => 'Train departing',
    ml: () => 'ട്രെയിൻ പുറപ്പെടുന്നു',
    hi: () => 'ट्रेन रवाना हो रही है'
  },
  [ALERT_TYPES.URGENT]: {
    en: () => 'Urgent announcement',
    ml: () => 'അടിയന്തര അറിയിപ്പ്',
    hi: () => 'तत्काल सूचना'
  },
  [ALERT_TYPES.CANCELLED]: {
    en: () => 'Train cancelled',
    ml: () => 'ട്രെയിൻ റദ്ദാക്കി',
    hi: () => 'ट्रेन रद्द है'
  }
};

// Priority keywords: URGENT > CANCELLED > DELAY > PLATFORM > DEPARTURE
const KEYWORD_PATTERNS = [
  {
    type: ALERT_TYPES.URGENT,
    regexes: [
      /\b(urgent|emergency)\b/i,
      /(അടിയന്തര|അടിയന്തിര)/i,
      /(आपात|तत्काल)/i
    ]
  },
  {
    type: ALERT_TYPES.CANCELLED,
    regexes: [
      /\b(cancel|canceled|cancelled|cancellation)\b/i,
      /(റദ്ദ്|റദ്ദാക്കി|റദ്ദാക്കപ്പെട്ടു)/i,
      /(रद्द|निरस्त)/i
    ]
  },
  {
    type: ALERT_TYPES.DELAY,
    regexes: [
      /\b(delay|delayed|delaying|late)\b/i,
      /(വൈകി|വൈകും|വൈകുന്നു|താമസം)/i,
      /(देरी|विलंब)/i
    ]
  },
  {
    type: ALERT_TYPES.PLATFORM,
    regexes: [
      /\bplatform\b/i,
      /(പ്ലാറ്റ്ഫോം|പ്ലാറ്റ്ഫോമിൽ)/i,
      /(प्लेटफ़ॉर्म|प्लेटफार्म)/i
    ]
  },
  {
    type: ALERT_TYPES.DEPARTURE,
    regexes: [
      /\b(depart|departure|departing|boarding|leaving)\b/i,
      /(പുറപ്പെടും|പുറപ്പെടുന്നു|യാത്ര തിരിക്കും)/i,
      /(रवाना|प्रस्थान)/i
    ]
  }
];

// Number word dictionaries for platforms 1 to 5
const NUMBER_WORDS = [
  { num: 1, words: ['1', 'one', 'ഒന്ന്', 'एक'] },
  { num: 2, words: ['2', 'two', 'രണ്ട്', 'दो'] },
  { num: 3, words: ['3', 'three', 'മൂന്ന്', 'तीन'] },
  { num: 4, words: ['4', 'four', 'നാല്', 'चार'] },
  { num: 5, words: ['5', 'five', 'അഞ്ച്', 'पांच', 'पाँच'] }
];

/**
 * Extracts platform number (1..5) from text.
 * Looks for number following or preceding the platform keyword, or general occurrences.
 */
export function extractPlatformNumber(text) {
  if (!text) return 0;
  const lower = text.toLowerCase();

  // Try direct digit match first (1 to 5)
  const digitMatch = lower.match(/(?:platform|പ്ലാറ്റ്ഫോം|प्लेटफ़ॉर्म|प्लेटफार्म)?\s*([1-5])\b/i);
  if (digitMatch && digitMatch[1]) {
    return parseInt(digitMatch[1], 10);
  }

  // Check word dictionary
  for (const item of NUMBER_WORDS) {
    for (const w of item.words) {
      if (typeof w === 'string') {
        const escaped = w.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
        // Word boundary for ASCII or direct substring for Indic scripts
        const re = /[a-z0-9]/i.test(w)
          ? new RegExp(`\\b${escaped}\\b`, 'i')
          : new RegExp(escaped, 'i');
        if (re.test(lower)) {
          return item.num;
        }
      }
    }
  }

  return 0;
}

/**
 * Matches an input text transcript to an EchoBand alert.
 * Returns an object with matched status, type, arg, and multilingual labels.
 */
export function matchAnnouncement(text) {
  if (!text || typeof text !== 'string' || text.trim() === '') {
    return {
      matched: false,
      type: ALERT_TYPES.NONE,
      typeName: 'NONE',
      arg: 0,
      text: text || '',
      labels: null
    };
  }

  const clean = text.trim();

  // Check categories in strict priority order
  for (const entry of KEYWORD_PATTERNS) {
    for (const regex of entry.regexes) {
      if (regex.test(clean)) {
        let arg = 0;
        if (entry.type === ALERT_TYPES.PLATFORM) {
          arg = extractPlatformNumber(clean);
        }

        const labelFormatter = ALERT_LABELS[entry.type];
        return {
          matched: true,
          type: entry.type,
          typeName: ALERT_NAMES[entry.type],
          arg,
          text: clean,
          labels: {
            en: labelFormatter.en(arg),
            ml: labelFormatter.ml(arg),
            hi: labelFormatter.hi(arg)
          }
        };
      }
    }
  }

  return {
    matched: false,
    type: ALERT_TYPES.NONE,
    typeName: 'NONE',
    arg: 0,
    text: clean,
    labels: null
  };
}
