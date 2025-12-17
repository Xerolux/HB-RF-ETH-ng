#!/usr/bin/env python3
"""
Add DTLS translations to all language files
"""

import os
import re

# DTLS translations for each language
dtls_blocks = {
    'fr.js': """
    // DTLS Security Settings
    dtlsSettings: 'Chiffrement DTLS',
    dtlsDescription: 'Chiffrement de transport sécurisé pour la communication entre la carte et CCU sur le port 3008.',
    dtlsMode: 'Mode de chiffrement',
    dtlsModeDisabled: 'Désactivé (par défaut)',
    dtlsModePsk: 'Clé prépartagée (PSK)',
    dtlsModeCertificate: 'Certificat X.509',
    dtlsCipherSuite: 'Suite de chiffrement',
    dtlsCipherAes128: 'AES-128-GCM-SHA256',
    dtlsCipherAes256: 'AES-256-GCM-SHA384 (Recommandé)',
    dtlsCipherChacha: 'ChaCha20-Poly1305-SHA256',
    dtlsRequireClientCert: 'Certificat client requis',
    dtlsSessionResumption: 'Activer la reprise de session',
    dtlsPskManagement: 'Gestion PSK',
    dtlsPskIdentity: 'Identité PSK',
    dtlsPskKey: 'Clé PSK (Hex)',
    dtlsPskGenerate: 'Générer une nouvelle PSK',
    dtlsPskGenerating: 'Génération PSK...',
    dtlsPskGenerated: 'Nouvelle PSK générée',
    dtlsPskCopyWarning: 'IMPORTANT: Copiez cette clé maintenant ! Elle ne sera affichée qu\\'une seule fois.',
    dtlsPskKeyLength: 'Longueur de clé',
    dtlsPsk128bit: '128 Bit',
    dtlsPsk256bit: '256 Bit (Recommandé)',
    dtlsPsk384bit: '384 Bit',
    dtlsPsk512bit: '512 Bit',
    dtlsPskStatus: 'Statut PSK',
    dtlsPskConfigured: 'Configuré',
    dtlsPskNotConfigured: 'Non configuré',
    dtlsWarningDisabled: 'La communication n\\'est PAS CHIFFRÉE. Tout le monde sur le réseau peut l\\'intercepter.',
    dtlsWarningPsk: 'Assurez-vous que le PSK est stocké en toute sécurité sur la CCU.',
    dtlsInfo: 'DTLS 1.2 chiffre la communication Raw-UART UDP de bout en bout. La CCU doit également prendre en charge DTLS.',
    dtlsDocumentation: 'Documentation pour les développeurs CCU',
    dtlsViewDocs: 'Voir le guide d\\'implémentation',
""",
    'es.js': """
    // DTLS Security Settings
    dtlsSettings: 'Cifrado DTLS',
    dtlsDescription: 'Cifrado de transporte seguro para la comunicación entre la placa y CCU en el puerto 3008.',
    dtlsMode: 'Modo de cifrado',
    dtlsModeDisabled: 'Deshabilitado (predeterminado)',
    dtlsModePsk: 'Clave precompartida (PSK)',
    dtlsModeCertificate: 'Certificado X.509',
    dtlsCipherSuite: 'Suite de cifrado',
    dtlsCipherAes128: 'AES-128-GCM-SHA256',
    dtlsCipherAes256: 'AES-256-GCM-SHA384 (Recomendado)',
    dtlsCipherChacha: 'ChaCha20-Poly1305-SHA256',
    dtlsRequireClientCert: 'Requerir certificado de cliente',
    dtlsSessionResumption: 'Habilitar reanudación de sesión',
    dtlsPskManagement: 'Gestión PSK',
    dtlsPskIdentity: 'Identidad PSK',
    dtlsPskKey: 'Clave PSK (Hex)',
    dtlsPskGenerate: 'Generar nueva PSK',
    dtlsPskGenerating: 'Generando PSK...',
    dtlsPskGenerated: 'Nueva PSK generada',
    dtlsPskCopyWarning: 'IMPORTANTE: ¡Copie esta clave ahora! Solo se mostrará una vez.',
    dtlsPskKeyLength: 'Longitud de clave',
    dtlsPsk128bit: '128 Bit',
    dtlsPsk256bit: '256 Bit (Recomendado)',
    dtlsPsk384bit: '384 Bit',
    dtlsPsk512bit: '512 Bit',
    dtlsPskStatus: 'Estado PSK',
    dtlsPskConfigured: 'Configurado',
    dtlsPskNotConfigured: 'No configurado',
    dtlsWarningDisabled: 'La comunicación NO ESTÁ CIFRADA. Cualquiera en la red puede interceptar el tráfico.',
    dtlsWarningPsk: 'Asegúrese de que el PSK se almacene de forma segura en la CCU.',
    dtlsInfo: 'DTLS 1.2 cifra la comunicación Raw-UART UDP de extremo a extremo. La CCU también debe admitir DTLS.',
    dtlsDocumentation: 'Documentación para desarrolladores de CCU',
    dtlsViewDocs: 'Ver guía de implementación',
""",
    'it.js': """
    // DTLS Security Settings
    dtlsSettings: 'Crittografia DTLS',
    dtlsDescription: 'Crittografia di trasporto sicura per la comunicazione tra scheda e CCU sulla porta 3008.',
    dtlsMode: 'Modalità di crittografia',
    dtlsModeDisabled: 'Disabilitato (predefinito)',
    dtlsModePsk: 'Chiave pre-condivisa (PSK)',
    dtlsModeCertificate: 'Certificato X.509',
    dtlsCipherSuite: 'Suite di cifratura',
    dtlsCipherAes128: 'AES-128-GCM-SHA256',
    dtlsCipherAes256: 'AES-256-GCM-SHA384 (Consigliato)',
    dtlsCipherChacha: 'ChaCha20-Poly1305-SHA256',
    dtlsRequireClientCert: 'Richiedere certificato client',
    dtlsSessionResumption: 'Abilita ripresa sessione',
    dtlsPskManagement: 'Gestione PSK',
    dtlsPskIdentity: 'Identità PSK',
    dtlsPskKey: 'Chiave PSK (Hex)',
    dtlsPskGenerate: 'Genera nuova PSK',
    dtlsPskGenerating: 'Generazione PSK...',
    dtlsPskGenerated: 'Nuova PSK generata',
    dtlsPskCopyWarning: 'IMPORTANTE: Copia questa chiave ora! Verrà mostrata solo una volta.',
    dtlsPskKeyLength: 'Lunghezza chiave',
    dtlsPsk128bit: '128 Bit',
    dtlsPsk256bit: '256 Bit (Consigliato)',
    dtlsPsk384bit: '384 Bit',
    dtlsPsk512bit: '512 Bit',
    dtlsPskStatus: 'Stato PSK',
    dtlsPskConfigured: 'Configurato',
    dtlsPskNotConfigured: 'Non configurato',
    dtlsWarningDisabled: 'La comunicazione NON È CRITTOGRAFATA. Chiunque sulla rete può intercettare il traffico.',
    dtlsWarningPsk: 'Assicurati che il PSK sia memorizzato in modo sicuro sulla CCU.',
    dtlsInfo: 'DTLS 1.2 crittografa la comunicazione Raw-UART UDP end-to-end. Anche la CCU deve supportare DTLS.',
    dtlsDocumentation: 'Documentazione per sviluppatori CCU',
    dtlsViewDocs: 'Visualizza guida all\\'implementazione',
""",
    'nl.js': """
    // DTLS Security Settings
    dtlsSettings: 'DTLS Versleuteling',
    dtlsDescription: 'Veilige transportversleuteling voor communicatie tussen board en CCU op poort 3008.',
    dtlsMode: 'Versleutelingsmodus',
    dtlsModeDisabled: 'Uitgeschakeld (standaard)',
    dtlsModePsk: 'Vooraf gedeelde sleutel (PSK)',
    dtlsModeCertificate: 'X.509 Certificaat',
    dtlsCipherSuite: 'Cipher Suite',
    dtlsCipherAes128: 'AES-128-GCM-SHA256',
    dtlsCipherAes256: 'AES-256-GCM-SHA384 (Aanbevolen)',
    dtlsCipherChacha: 'ChaCha20-Poly1305-SHA256',
    dtlsRequireClientCert: 'Clientcertificaat vereisen',
    dtlsSessionResumption: 'Sessiehervatting inschakelen',
    dtlsPskManagement: 'PSK Beheer',
    dtlsPskIdentity: 'PSK Identiteit',
    dtlsPskKey: 'PSK Sleutel (Hex)',
    dtlsPskGenerate: 'Nieuwe PSK genereren',
    dtlsPskGenerating: 'PSK genereren...',
    dtlsPskGenerated: 'Nieuwe PSK gegenereerd',
    dtlsPskCopyWarning: 'BELANGRIJK: Kopieer deze sleutel nu! Hij wordt slechts één keer getoond.',
    dtlsPskKeyLength: 'Sleutellengte',
    dtlsPsk128bit: '128 Bit',
    dtlsPsk256bit: '256 Bit (Aanbevolen)',
    dtlsPsk384bit: '384 Bit',
    dtlsPsk512bit: '512 Bit',
    dtlsPskStatus: 'PSK Status',
    dtlsPskConfigured: 'Geconfigureerd',
    dtlsPskNotConfigured: 'Niet geconfigureerd',
    dtlsWarningDisabled: 'Communicatie is ONVERSLEUTELD. Iedereen op het netwerk kan verkeer onderscheppen.',
    dtlsWarningPsk: 'Zorg ervoor dat de PSK veilig is opgeslagen op de CCU.',
    dtlsInfo: 'DTLS 1.2 versleutelt Raw-UART UDP-communicatie end-to-end. De CCU moet ook DTLS ondersteunen.',
    dtlsDocumentation: 'Documentatie voor CCU-ontwikkelaars',
    dtlsViewDocs: 'Implementatiegids bekijken',
""",
    'pl.js': """
    // DTLS Security Settings
    dtlsSettings: 'Szyfrowanie DTLS',
    dtlsDescription: 'Bezpieczne szyfrowanie transportu dla komunikacji między płytką a CCU na porcie 3008.',
    dtlsMode: 'Tryb szyfrowania',
    dtlsModeDisabled: 'Wyłączony (domyślnie)',
    dtlsModePsk: 'Klucz współdzielony (PSK)',
    dtlsModeCertificate: 'Certyfikat X.509',
    dtlsCipherSuite: 'Zestaw szyfrów',
    dtlsCipherAes128: 'AES-128-GCM-SHA256',
    dtlsCipherAes256: 'AES-256-GCM-SHA384 (Zalecane)',
    dtlsCipherChacha: 'ChaCha20-Poly1305-SHA256',
    dtlsRequireClientCert: 'Wymagaj certyfikatu klienta',
    dtlsSessionResumption: 'Włącz wznowienie sesji',
    dtlsPskManagement: 'Zarządzanie PSK',
    dtlsPskIdentity: 'Tożsamość PSK',
    dtlsPskKey: 'Klucz PSK (Hex)',
    dtlsPskGenerate: 'Wygeneruj nowy PSK',
    dtlsPskGenerating: 'Generowanie PSK...',
    dtlsPskGenerated: 'Nowy PSK wygenerowany',
    dtlsPskCopyWarning: 'WAŻNE: Skopiuj ten klucz teraz! Zostanie wyświetlony tylko raz.',
    dtlsPskKeyLength: 'Długość klucza',
    dtlsPsk128bit: '128 Bit',
    dtlsPsk256bit: '256 Bit (Zalecane)',
    dtlsPsk384bit: '384 Bit',
    dtlsPsk512bit: '512 Bit',
    dtlsPskStatus: 'Status PSK',
    dtlsPskConfigured: 'Skonfigurowany',
    dtlsPskNotConfigured: 'Nieskonfigurowany',
    dtlsWarningDisabled: 'Komunikacja jest NIESZYFROWANA. Każdy w sieci może przechwycić ruch.',
    dtlsWarningPsk: 'Upewnij się, że PSK jest bezpiecznie przechowywany na CCU.',
    dtlsInfo: 'DTLS 1.2 szyfruje komunikację Raw-UART UDP end-to-end. CCU musi również obsługiwać DTLS.',
    dtlsDocumentation: 'Dokumentacja dla programistów CCU',
    dtlsViewDocs: 'Zobacz przewodnik implementacji',
""",
    'cs.js': """
    // DTLS Security Settings
    dtlsSettings: 'Šifrování DTLS',
    dtlsDescription: 'Bezpečné šifrování přenosu pro komunikaci mezi deskou a CCU na portu 3008.',
    dtlsMode: 'Režim šifrování',
    dtlsModeDisabled: 'Zakázáno (výchozí)',
    dtlsModePsk: 'Předsdílený klíč (PSK)',
    dtlsModeCertificate: 'Certifikát X.509',
    dtlsCipherSuite: 'Šifrovací sada',
    dtlsCipherAes128: 'AES-128-GCM-SHA256',
    dtlsCipherAes256: 'AES-256-GCM-SHA384 (Doporučeno)',
    dtlsCipherChacha: 'ChaCha20-Poly1305-SHA256',
    dtlsRequireClientCert: 'Vyžadovat klientský certifikát',
    dtlsSessionResumption: 'Povolit obnovení relace',
    dtlsPskManagement: 'Správa PSK',
    dtlsPskIdentity: 'Identita PSK',
    dtlsPskKey: 'Klíč PSK (Hex)',
    dtlsPskGenerate: 'Generovat nový PSK',
    dtlsPskGenerating: 'Generování PSK...',
    dtlsPskGenerated: 'Nový PSK vygenerován',
    dtlsPskCopyWarning: 'DŮLEŽITÉ: Zkopírujte tento klíč nyní! Bude zobrazen pouze jednou.',
    dtlsPskKeyLength: 'Délka klíče',
    dtlsPsk128bit: '128 Bit',
    dtlsPsk256bit: '256 Bit (Doporučeno)',
    dtlsPsk384bit: '384 Bit',
    dtlsPsk512bit: '512 Bit',
    dtlsPskStatus: 'Stav PSK',
    dtlsPskConfigured: 'Nakonfigurováno',
    dtlsPskNotConfigured: 'Nenakonfigurováno',
    dtlsWarningDisabled: 'Komunikace je NEŠIFROVANÁ. Kdokoli v síti může zachytit provoz.',
    dtlsWarningPsk: 'Ujistěte se, že PSK je bezpečně uložen na CCU.',
    dtlsInfo: 'DTLS 1.2 šifruje komunikaci Raw-UART UDP end-to-end. CCU musí také podporovat DTLS.',
    dtlsDocumentation: 'Dokumentace pro vývojáře CCU',
    dtlsViewDocs: 'Zobrazit průvodce implementací',
""",
    'sv.js': """
    // DTLS Security Settings
    dtlsSettings: 'DTLS Kryptering',
    dtlsDescription: 'Säker transportkryptering för kommunikation mellan kort och CCU på port 3008.',
    dtlsMode: 'Krypteringsläge',
    dtlsModeDisabled: 'Inaktiverad (standard)',
    dtlsModePsk: 'Fördelad nyckel (PSK)',
    dtlsModeCertificate: 'X.509 Certifikat',
    dtlsCipherSuite: 'Chiffer Suite',
    dtlsCipherAes128: 'AES-128-GCM-SHA256',
    dtlsCipherAes256: 'AES-256-GCM-SHA384 (Rekommenderas)',
    dtlsCipherChacha: 'ChaCha20-Poly1305-SHA256',
    dtlsRequireClientCert: 'Kräv klientcertifikat',
    dtlsSessionResumption: 'Aktivera sessionsåterupptagning',
    dtlsPskManagement: 'PSK Hantering',
    dtlsPskIdentity: 'PSK Identitet',
    dtlsPskKey: 'PSK Nyckel (Hex)',
    dtlsPskGenerate: 'Generera ny PSK',
    dtlsPskGenerating: 'Genererar PSK...',
    dtlsPskGenerated: 'Ny PSK genererad',
    dtlsPskCopyWarning: 'VIKTIGT: Kopiera denna nyckel nu! Den visas bara en gång.',
    dtlsPskKeyLength: 'Nyckellängd',
    dtlsPsk128bit: '128 Bit',
    dtlsPsk256bit: '256 Bit (Rekommenderas)',
    dtlsPsk384bit: '384 Bit',
    dtlsPsk512bit: '512 Bit',
    dtlsPskStatus: 'PSK Status',
    dtlsPskConfigured: 'Konfigurerad',
    dtlsPskNotConfigured: 'Inte konfigurerad',
    dtlsWarningDisabled: 'Kommunikationen är OKRYPTERAD. Vem som helst på nätverket kan fånga upp trafiken.',
    dtlsWarningPsk: 'Se till att PSK lagras säkert på CCU.',
    dtlsInfo: 'DTLS 1.2 krypterar Raw-UART UDP-kommunikation end-to-end. CCU måste också stödja DTLS.',
    dtlsDocumentation: 'Dokumentation för CCU-utvecklare',
    dtlsViewDocs: 'Visa implementeringsguide',
""",
    'no.js': """
    // DTLS Security Settings
    dtlsSettings: 'DTLS Kryptering',
    dtlsDescription: 'Sikker transportkryptering for kommunikasjon mellom kort og CCU på port 3008.',
    dtlsMode: 'Krypteringsmodus',
    dtlsModeDisabled: 'Deaktivert (standard)',
    dtlsModePsk: 'Forhåndsdelt nøkkel (PSK)',
    dtlsModeCertificate: 'X.509 Sertifikat',
    dtlsCipherSuite: 'Cipher Suite',
    dtlsCipherAes128: 'AES-128-GCM-SHA256',
    dtlsCipherAes256: 'AES-256-GCM-SHA384 (Anbefalt)',
    dtlsCipherChacha: 'ChaCha20-Poly1305-SHA256',
    dtlsRequireClientCert: 'Krev klientsertifikat',
    dtlsSessionResumption: 'Aktiver øktgjenopptagelse',
    dtlsPskManagement: 'PSK Administrasjon',
    dtlsPskIdentity: 'PSK Identitet',
    dtlsPskKey: 'PSK Nøkkel (Hex)',
    dtlsPskGenerate: 'Generer ny PSK',
    dtlsPskGenerating: 'Genererer PSK...',
    dtlsPskGenerated: 'Ny PSK generert',
    dtlsPskCopyWarning: 'VIKTIG: Kopier denne nøkkelen nå! Den vises bare én gang.',
    dtlsPskKeyLength: 'Nøkkellengde',
    dtlsPsk128bit: '128 Bit',
    dtlsPsk256bit: '256 Bit (Anbefalt)',
    dtlsPsk384bit: '384 Bit',
    dtlsPsk512bit: '512 Bit',
    dtlsPskStatus: 'PSK Status',
    dtlsPskConfigured: 'Konfigurert',
    dtlsPskNotConfigured: 'Ikke konfigurert',
    dtlsWarningDisabled: 'Kommunikasjonen er UKRYPTERT. Alle på nettverket kan fange opp trafikken.',
    dtlsWarningPsk: 'Sørg for at PSK lagres sikkert på CCU.',
    dtlsInfo: 'DTLS 1.2 krypterer Raw-UART UDP-kommunikasjon ende-til-ende. CCU må også støtte DTLS.',
    dtlsDocumentation: 'Dokumentasjon for CCU-utviklere',
    dtlsViewDocs: 'Vis implementeringsveiledning',
"""
}

def add_dtls_to_file(filename, dtls_block):
    """Add DTLS translations to a language file"""
    filepath = os.path.join('webui', 'src', 'locales', filename)

    if not os.path.exists(filepath):
        print(f"File not found: {filepath}")
        return False

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # Check if DTLS is already present
    if 'dtlsSettings' in content:
        print(f"DTLS already present in {filename}, skipping...")
        return True

    # Find the language line and insert DTLS block after it
    pattern = r"(language: '[^']+',)\n"
    replacement = r"\1\n" + dtls_block + "\n"

    new_content = re.sub(pattern, replacement, content)

    if new_content == content:
        print(f"Could not find insertion point in {filename}")
        return False

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(new_content)

    print(f"[OK] Added DTLS translations to {filename}")
    return True

# Process all files
for filename, dtls_block in dtls_blocks.items():
    add_dtls_to_file(filename, dtls_block)

print("\n[OK] All language files updated with DTLS translations!")
