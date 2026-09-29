# Draxo Keygen — Deployment Guide

## Important: GitHub Pages does NOT support PHP

`batotomato.github.io` is a **static** host - it can only serve HTML/CSS/JS.
`keygen.php` will NOT work there.

You have two options:

| Option | File | Host | Rate Limiting |
|--------|------|------|---------------|
| **A** | `keygen.html` | GitHub Pages (free) | localStorage |
| **B** | `keygen.php` | PHP host (free/paid) | Server-side file |

**Recommendation: Start with Option A (quick, free).**

---

## OPTION A: Deploy keygen.html (RECOMMENDED)

### Step 1: Edit your Linkvertise links

Open `tools/keygen.html`. Find `LINKVERTISE_LINKS` near the bottom:

```
const LINKVERTISE_LINKS=[
    'https://linkvertise.com/YOUR_ID_1/draxo-ad1?hwid={HWID}',
    'https://linkvertise.com/YOUR_ID_2/draxo-ad2?hwid={HWID}',
    'https://linkvertise.com/YOUR_ID_3/draxo-ad3?hwid={HWID}',
];
```

Replace `YOUR_ID_1/2/3` with your actual Linkvertise IDs. Keep `?hwid={HWID}`.

### Step 2: Copy to your GitHub Pages repo

```bash
cd /path/to/batotomato.github.io
mkdir -p draxo
cp "C:/Draxo Client/tools/keygen.html" draxo/
git add draxo/keygen.html
git commit -m "Add Draxo license key generator"
git push
```

### Step 3: Test

1. Open `https://batotomato.github.io/draxo/keygen.html?check`
   Should show: **"Self-check PASSED"**

2. Paste a real HWID → 3 ads appear → click "Generate Key" → copy key

3. Activate in Draxo: CONFIG → LICENSE → paste → Activate

### How the flow works

1. User visits your page, enters HWID
2. 3 Linkvertise ad links appear with their HWID embedded
3. User completes all 3 ads (each opens in new tab)
4. User clicks "Generate Key" → 24h key generated in browser
5. User copies and activates in Draxo menu

---

## OPTION B: Deploy keygen.php (require a PHP host)

For stronger rate limiting. Free PHP hosts: InfinityFree, 000webhost.

### Step 1: Edit CONFIGURATION in keygen.php (lines 23-63):

```php
$ENFORCE_LINKVERTISE = false;  // set to true
$LINKVERTISE_API_KEY = 'YOUR_KEY_HERE';
$LINKVERTISE_LINKS = [...your links...];
$KEYGEN_URL = 'https://your-php-host.com/keygen.php';
```

### Step 2: Upload + test

Upload to your PHP host, then:
- `https://your-host/keygen.php?check` → "OK: Self-check passed"
- `https://your-host/keygen.php?hwid=0123...CDEF` → generates key

---

## Local testing (no deployment needed)

```bash
# Test Python reference
cd "C:/Draxo Client"
python tools/keygen_reference.py
# Output: DRAXO-5YQZZ-4VAK1-8FJGN-XTDDG-A9076-M

# Test keygen.html (open in browser)
start "C:/Draxo Client/tools/keygen.html?check"

# Test keygen.php (if PHP installed)
cd "C:/Draxo Client/tools"
php -S localhost:8000
# Open http://localhost:8000/keygen.php?check
```
