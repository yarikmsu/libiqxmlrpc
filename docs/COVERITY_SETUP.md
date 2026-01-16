# Coverity Scan Setup Guide

Coverity Scan provides deep static analysis for free to open source projects.

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
