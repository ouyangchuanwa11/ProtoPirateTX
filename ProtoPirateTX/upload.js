const fs = require('fs');
const path = require('path');
const https = require('https');

const TOKEN = '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';
const REPO = 'ouyangchuanwa11/ProtoPirateTX';
const BASE_DIR = 'F:\\U-Claw-Data\\.openclaw\\workspace\\ProtoPirateTX';
const COMMIT_MSG = 'Update to v3.1 - TX enabled, RollBack attack, 10 protocols';

const files = [
  'application.fam',
  'protopirate_rb.h',
  'protopirate_rb.c',
  'protopirate_decoder.c',
  'protopirate_tx.c',
  'protopirate_rollback.c',
  '.github/workflows/build.yml'
];

function apiRequest(method, urlPath, body) {
  return new Promise((resolve, reject) => {
    const options = {
      hostname: 'api.github.com',
      path: urlPath,
      method: method,
      headers: {
        'Authorization': `Bearer ${TOKEN}`,
        'Accept': 'application/vnd.github+json',
        'User-Agent': 'ProtoPirateTX-Upload',
        'Content-Type': 'application/json'
      }
    };
    
    const req = https.request(options, (res) => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => {
        try {
          const json = JSON.parse(data);
          resolve({ status: res.statusCode, data: json });
        } catch (e) {
          resolve({ status: res.statusCode, data: data });
        }
      });
    });
    
    req.on('error', reject);
    if (body) req.write(JSON.stringify(body));
    req.end();
  });
}

async function getFileSha(filePath) {
  const urlPath = `/repos/${REPO}/contents/${encodeURIComponent(filePath)}`;
  const result = await apiRequest('GET', urlPath);
  if (result.status === 200 && result.data.sha) {
    return result.data.sha;
  }
  if (result.status === 404) {
    return null; // file doesn't exist yet
  }
  throw new Error(`Failed to get SHA for ${filePath}: ${result.status} ${JSON.stringify(result.data)}`);
}

async function updateFile(filePath, content, sha) {
  const urlPath = `/repos/${REPO}/contents/${encodeURIComponent(filePath)}`;
  const base64Content = Buffer.from(content, 'utf-8').toString('base64');
  
  const body = {
    message: COMMIT_MSG,
    content: base64Content
  };
  
  if (sha) {
    body.sha = sha;
  }
  
  const result = await apiRequest('PUT', urlPath, body);
  return result;
}

async function main() {
  console.log('=== ProtoPirateTX GitHub Upload ===\n');
  
  for (const file of files) {
    try {
      const filePath = path.join(BASE_DIR, file);
      const content = fs.readFileSync(filePath, 'utf-8');
      
      console.log(`\n--- Processing: ${file} ---`);
      
      // Get current SHA (if file exists on GitHub)
      let sha = null;
      try {
        sha = await getFileSha(file);
        console.log(`  SHA: ${sha || 'NEW FILE'}`);
      } catch (e) {
        console.log(`  Error getting SHA: ${e.message}`);
        console.log('  Assuming new file...');
        sha = null;
      }
      
      // Update file
      const result = await updateFile(file, content, sha);
      
      if (result.status === 200 || result.status === 201) {
        console.log(`  ✅ SUCCESS (${result.status}): ${file}`);
        console.log(`     SHA: ${result.data.content?.sha || result.data.sha}`);
      } else {
        console.log(`  ❌ FAILED (${result.status}): ${file}`);
        console.log(`     Response: ${JSON.stringify(result.data)}`);
      }
      
    } catch (e) {
      console.log(`  ❌ ERROR: ${file} - ${e.message}`);
    }
  }
  
  console.log('\n=== Upload Complete ===');
}

main().catch(console.error);
