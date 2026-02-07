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
  { code: 'de', name: 'Deutsch' },
  { code: 'en', name: 'English' },
  { code: 'fr', name: 'Français' },
  { code: 'nl', name: 'Nederlands' },
  { code: 'it', name: 'Italiano' },
  { code: 'es', name: 'Español' },
  { code: 'pl', name: 'Polski' },
  { code: 'cs', name: 'Čeština' },
  { code: 'no', name: 'Norsk' },
  { code: 'sv', name: 'Svenska' }
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
