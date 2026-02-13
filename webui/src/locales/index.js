import de from './de.js'
import en from './en.js'
import fr from './fr.js'
import nl from './nl.js'
import it from './it.js'
import es from './es.js'
import pl from './pl.js'
import cs from './cs.js'
import no from './no.js'
import sv from './sv.js'

export const messages = {
  de,
  en,
  fr,
  nl,
  it,
  es,
  pl,
  cs,
  no,
  sv
}

export const availableLocales = [
  { code: 'de', name: 'Deutsch', flag: '\uD83C\uDDE9\uD83C\uDDEA' },
  { code: 'en', name: 'English', flag: '\uD83C\uDDEC\uD83C\uDDE7' },
  { code: 'fr', name: 'Fran\u00E7ais', flag: '\uD83C\uDDEB\uD83C\uDDF7' },
  { code: 'nl', name: 'Nederlands', flag: '\uD83C\uDDF3\uD83C\uDDF1' },
  { code: 'it', name: 'Italiano', flag: '\uD83C\uDDEE\uD83C\uDDF9' },
  { code: 'es', name: 'Espa\u00F1ol', flag: '\uD83C\uDDEA\uD83C\uDDF8' },
  { code: 'pl', name: 'Polski', flag: '\uD83C\uDDF5\uD83C\uDDF1' },
  { code: 'cs', name: '\u010Ce\u0161tina', flag: '\uD83C\uDDE8\uD83C\uDDFF' },
  { code: 'no', name: 'Norsk', flag: '\uD83C\uDDF3\uD83C\uDDF4' },
  { code: 'sv', name: 'Svenska', flag: '\uD83C\uDDF8\uD83C\uDDEA' }
]

// Function to get browser language and map it to available locale
export function getBrowserLocale() {
  const browserLang = navigator.language || navigator.userLanguage
  const langCode = browserLang.split('-')[0]

  // Check if we have this language
  if (messages[langCode]) {
    return langCode
  }

  // Default to English
  return 'en'
}
