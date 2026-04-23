/**
 * API Helper utilities for consistent error handling and response validation
 */

/**
 * Validate API response has required data structure
 * @param {Object} response - Axios response object
 * @param {string|string[]} requiredFields - Field(s) to check in response.data
 * @returns {boolean} True if response is valid
 */
export function validateResponse(response, requiredFields = []) {
  if (!response?.data) return false

  if (!Array.isArray(requiredFields)) {
    requiredFields = [requiredFields]
  }

  return requiredFields.every(field => field in response.data)
}

/**
 * Get user-friendly error message from axios error
 * @param {Error} error - Axios error object
 * @returns {string} User-friendly error message
 */
export function getErrorMessage(error) {
  if (error.response?.status === 401) {
    return 'Authentication failed. Please log in again.'
  }

  if (error.response?.status === 400) {
    return error.response?.data?.error || 'Invalid request. Please check your input.'
  }

  if (error.response?.status >= 500) {
    return `Server error (${error.response.status}). Please try again later.`
  }

  if (error.code === 'ECONNABORTED') {
    return 'Request timeout. Please check your connection and try again.'
  }

  if (error.message === 'Network Error' || !error.response) {
    return 'Network error. Unable to reach the device.'
  }

  return error.message || 'An error occurred. Please try again.'
}

/**
 * Safely get nested property from object
 * @param {Object} obj - Object to query
 * @param {string} path - Dot-separated path (e.g., 'data.user.name')
 * @param {*} defaultValue - Default value if path not found
 * @returns {*} Value at path or defaultValue
 */
export function getNestedProperty(obj, path, defaultValue = null) {
  return path.split('.').reduce((current, prop) => current?.[prop], obj) ?? defaultValue
}

/**
 * Safe JSON parse with fallback
 * @param {string} jsonStr - JSON string to parse
 * @param {*} fallback - Fallback value if parsing fails
 * @returns {*} Parsed JSON or fallback
 */
export function safeJsonParse(jsonStr, fallback = null) {
  try {
    return JSON.parse(jsonStr)
  } catch (e) {
    console.error('JSON parse error:', e)
    return fallback
  }
}
