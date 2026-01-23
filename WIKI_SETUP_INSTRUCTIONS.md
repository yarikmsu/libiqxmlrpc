# Wiki Setup Instructions

This guide walks you through enabling and publishing the fork's wiki documentation.

## Prerequisites

You need admin access to the GitHub repository: https://github.com/yarikmsu/libiqxmlrpc

## Step 1: Enable Wiki on GitHub

1. Navigate to: https://github.com/yarikmsu/libiqxmlrpc/settings
2. Scroll down to the **"Features"** section
3. Check the **"Wikis"** checkbox
4. Click **"Save changes"**

## Step 2: Clone the Wiki Repository

Once enabled, the wiki becomes a separate Git repository:

```bash
# Clone the wiki repo (not the main repo)
git clone https://github.com/yarikmsu/libiqxmlrpc.wiki.git

# Navigate into it
cd libiqxmlrpc.wiki
```

## Step 3: Copy Wiki Content

The wiki content has been prepared in the `wiki_content/` directory of the main repository. Copy all files:

```bash
# From inside libiqxmlrpc.wiki/ directory
cp ../libiqxmlrpc/wiki_content/*.md .

# Verify files are present
ls -la
# Should see: Home.md, Quick-Start.md, Building.md, Performance-Results.md, Whats-New.md
```

## Step 4: Commit and Push

```bash
git add Home.md Quick-Start.md Building.md Performance-Results.md Whats-New.md

git commit -m "Add fork-specific wiki documentation

- Home: Fork overview with links to upstream and internal docs
- Quick Start: 5-minute getting started guide for new users
- Building: Updated requirements (C++17, OpenSSL 1.1.0+, CMake 3.10+)
- Performance Results: Summary of 28 optimizations (1.2x-12.5x faster)
- What's New: Migration guide from upstream with new APIs

References upstream wiki for stable content (Value API, Client/Server guides).
Links to /docs/ for advanced topics (Performance, Security, Development)."

git push origin master
```

## Step 5: Verify

1. Visit: https://github.com/yarikmsu/libiqxmlrpc/wiki
2. Verify all 5 pages are visible:
   - **Home** - Landing page
   - **Quick-Start** - Getting started guide
   - **Building** - Build instructions
   - **Performance-Results** - Optimization highlights
   - **Whats-New** - Migration guide

3. Check that:
   - Links to upstream wiki work (e.g., Writing-Client)
   - Links to `/docs/` internal guides work
   - Code examples render properly
   - Tables display correctly

## Step 6: Cleanup (Optional)

After successfully publishing the wiki, you can remove the temporary `wiki_content/` directory from the main repository:

```bash
# From main repo directory
cd ../libiqxmlrpc
rm -rf wiki_content/
git add -u
git commit -m "Remove wiki_content directory after wiki publication"
git push
```

You can also delete this instruction file:
```bash
rm WIKI_SETUP_INSTRUCTIONS.md
```

## Troubleshooting

### Wiki not appearing after enabling
- Wait a few minutes and refresh the page
- Ensure you have admin permissions on the repository

### git clone fails
- Make sure the wiki is enabled in repository settings
- The wiki URL should be: https://github.com/yarikmsu/libiqxmlrpc.wiki.git (note the `.wiki`)

### Links don't work
- Wiki internal links use page names without `.md` extension
- Example: `[Quick Start](Quick-Start)` links to `Quick-Start.md`
- Links to main repo files use full paths: `/docs/PERFORMANCE_GUIDE.md`

## Next Steps

After publishing:
1. Update the main README if not already done (should point to fork wiki)
2. Announce the new wiki in your next release notes
3. Consider updating the GitHub repository description to mention the wiki

## Maintenance

The wiki should need minimal updates. Quarterly or when major changes occur:
- Update Performance Results with new benchmarks
- Add new APIs to What's New
- Review upstream wiki for relevant updates to reference
