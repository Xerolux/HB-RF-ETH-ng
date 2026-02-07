# Security Notice

## Security Updates - December 2025

### Dependencies Updated

#### ✅ WebUI Dependencies Updated
- **Parcel**: Updated from `^2.12.0` to `^2.16.3`
- **vue-router**: Updated from `^4.6.3` to `^4.6.4`

### Known Security Issues

#### ⚠️ Parcel Development Server Vulnerability (CVE-2025-56648)

**Status**: UNPATCHED (as of December 12, 2025)

**Severity**: Moderate (CVSS 6.5)

**Description**:
The Parcel development server has an origin validation error vulnerability (GHSA-qm9p-f9j5-w83w) that affects versions 1.6.1 through 2.16.3. This vulnerability allows malicious websites to send XMLHTTPRequests to the local development server and potentially steal source code when developers visit those sites.

**Affected Component**: `@parcel/reporter-dev-server`

**Root Cause**:
1. Insecure CORS configuration (`Access-Control-Allow-Origin: *`)
2. Missing Host header validation
3. Vulnerability to DNS rebinding attacks

**Impact**:
- Only affects the **development environment**
- Production builds are NOT affected
- Risk exists only when developers run `npm run dev` and visit untrusted websites

**Mitigation**:
While no official patch is available yet, follow these security practices:

1. **Development Environment Security**:
   - Only visit trusted websites while running the development server
   - Stop the development server when not actively developing
   - Use a separate browser profile for development
   - Consider using a firewall to restrict access to localhost:1234

2. **Production Builds**:
   - Production builds (`npm run build`) are NOT affected
   - The vulnerability only exists in the development server

**Current Status**:
We have updated to the latest available version (2.16.3), which is currently the most recent release. We are monitoring for security patches from the Parcel team.

**References**:
- GitHub Advisory: https://github.com/advisories/GHSA-qm9p-f9j5-w83w
- CVE: CVE-2025-56648

---

## Firmware Security Recommendations

### Default Credentials

**IMPORTANT**: The firmware ships with a default admin password (`admin`).

**Action Required**:
- **IMMEDIATELY** change the default admin password after first boot
- Access the WebUI and navigate to Settings
- Set a strong, unique password
- Recommended: Use 12+ characters with mixed case, numbers, and symbols

### SNMP Configuration

The firmware includes SNMP monitoring with a default community string of `public`.

**Recommendations**:
- Change the default SNMP community string in production environments
- Use SNMPv3 with authentication if your monitoring system supports it
- Restrict SNMP access to trusted monitoring hosts only

### Network Security

**Best Practices**:
- Deploy the device on a trusted/isolated network segment
- Use firewall rules to restrict access to necessary ports only
- Regularly check for firmware updates
- Monitor the device logs for suspicious activity

### Authentication Token

The WebUI uses a secure token-based authentication system:
- Tokens are generated using ESP32's hardware random number generator
- Tokens are hashed using SHA-256
- Tokens are unique per device (includes serial number)
- Tokens expire on device reboot

---

## Reporting Security Issues

If you discover a security vulnerability in this project, please:

1. **DO NOT** open a public issue
2. Email the maintainer privately with details
3. Allow reasonable time for a fix before public disclosure
4. Follow responsible disclosure practices

---

## Security Update History

### 2025-12-12
- Updated WebUI dependencies (Parcel, vue-router)
- Added security documentation
- Identified and documented CVE-2025-56648

---

Last Updated: December 12, 2025
