# Coverity Scan Setup Guide

Coverity Scan provides deep static analysis for free to open source projects.

## Workflow Configuration

The Coverity Scan workflow is defined in `.github/workflows/coverity.yml`.

## Setup Steps

### 1. Register Project at Coverity Scan

1. Go to https://scan.coverity.com/projects/new
2. Select "Add project from GitHub"
3. Choose `yarikmsu/libiqxmlrpc`
4. Fill in project details

### 2. Get Your Token

1. Go to your project page on Coverity Scan
2. Click "Project Settings"
3. Copy the "Project Token"

**Security Note:** The project token allows anyone to submit builds to your Coverity project. Keep it confidential and only store it as a GitHub secret. Never commit it to the repository or expose it in logs.

### 3. Add GitHub Secrets

In your GitHub repository settings (Settings → Secrets and variables → Actions):

| Secret Name | Value |
|-------------|-------|
| `COVERITY_SCAN_TOKEN` | Your Coverity project token |
| `COVERITY_SCAN_EMAIL` | Email used for Coverity registration |

### 4. Verify Integration

1. Go to Actions → Coverity Scan workflow
2. Click "Run workflow" to trigger manually
3. Check results at https://scan.coverity.com/projects/yarikmsu-libiqxmlrpc

## When Does It Run?

| Trigger | Frequency |
|---------|-----------|
| Schedule | Weekly (Sunday 00:00 UTC) |
| Manual | On-demand via workflow_dispatch |
| Push | On changes to `libiqxmlrpc/**` on master |

## Submission Limits

Free projects have limited daily submissions. The workflow is configured to run weekly to stay within limits. Use manual trigger sparingly.

## Viewing Results

Results are available at:
- https://scan.coverity.com/projects/yarikmsu-libiqxmlrpc

Coverity provides:
- Defect reports with severity ratings
- Code paths showing how bugs occur
- Fix recommendations
- Trend analysis over time

## Troubleshooting

### "Failed to download Coverity tool"

- Verify `COVERITY_SCAN_TOKEN` is set correctly in repository secrets
- Check if the project is registered at https://scan.coverity.com
- Ensure the token hasn't been regenerated (update the secret if so)

### "Coverity submission failed with HTTP 4xx/5xx"

- HTTP 401/403: Token is invalid or expired
- HTTP 429: Submission quota exceeded - wait 24 hours
- HTTP 5xx: Coverity service issue - retry later

### "Coverity submission issue detected in response"

- Check if daily submission quota is exceeded (free tier has limits)
- Wait 24 hours or check quota at https://scan.coverity.com
- Consider reducing push-triggered runs by limiting path filters

### Workflow runs but no results appear

- Allow 15-30 minutes for Coverity to process the submission
- Check the email associated with `COVERITY_SCAN_EMAIL` for notifications
- Verify the project is properly configured at Coverity Scan dashboard

### Build capture shows few files

- Ensure `cov-build` is in PATH before running make
- Check `build/cov-int/build-log.txt` for capture details
- Verify the build actually compiled source files (not just linked)
