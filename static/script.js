// State Management
let problemsList = [];
let currentProblem = null;
let currentCategory = 'all';
let searchQuery = '';
let lastJudgeResult = null;
let openTestcaseIndex = null; // Track which testcase detail is expanded in modal

// API base URL — change this when frontend is on GitHub Pages and backend runs locally
// Empty string '' = same origin (normal mode)
// 'http://localhost:8080' = local backend when frontend is on GitHub Pages
let API_BASE = localStorage.getItem('api_base') || '';

// DOM Content Loaded
document.addEventListener('DOMContentLoaded', () => {
  initRouter();
  loadSettings();
  setupEditorKeyboardShortcuts();
  
  // Refresh problems list periodically (every 10s instead of 30s for quick sync feedback)
  setInterval(refreshProblemsListSilent, 10000);
});

// ==========================================================================
// Router (Hash-based)
// ==========================================================================
function initRouter() {
  const handleRoute = () => {
    const hash = window.location.hash || '#/problems';
    
    if (hash.startsWith('#/problem/')) {
      const id = hash.replace('#/problem/', '');
      viewProblemDetail(id);
    } else {
      viewProblemsList();
    }
  };

  window.addEventListener('hashchange', handleRoute);
  handleRoute(); // initial load
}

function navigateTo(target, id = '') {
  if (target === 'problems') {
    window.location.hash = '#/problems';
  } else if (target === 'problem' && id) {
    window.location.hash = `#/problem/${id}`;
  }
}

// ==========================================================================
// API Calls & Data Fetching
// ==========================================================================
async function fetchProblems() {
  try {
    const resp = await fetch(API_BASE + '/api/problems');
    if (!resp.ok) throw new Error('Không thể lấy danh sách bài tập');
    problemsList = await resp.json();
    return problemsList;
  } catch (e) {
    showToast(e.message, 'error');
    console.error(e);
    return [];
  }
}

async function fetchProblemDetail(id) {
  try {
    const resp = await fetch(API_BASE + `/api/problem/${id}`);
    if (!resp.ok) throw new Error('Không thể tải chi tiết bài tập');
    return await resp.json();
  } catch (e) {
    showToast(e.message, 'error');
    console.error(e);
    return null;
  }
}

async function reloadProblems() {
  const btn = document.querySelector('.btn-reload');
  const icon = document.getElementById('reload-icon');
  icon.classList.add('spinning');
  btn.disabled = true;
  
  try {
    const resp = await fetch(API_BASE + '/api/reload', { method: 'POST' });
    if (!resp.ok) throw new Error('Gửi yêu cầu đồng bộ thất bại');
    const data = await resp.json();
    
    showToast(`Đồng bộ thành công! Đã quét ${data.count} bài tập.`, 'success');
    await viewProblemsList(); // Refresh table
  } catch (e) {
    showToast(e.message, 'error');
  } finally {
    icon.classList.remove('spinning');
    btn.disabled = false;
  }
}

async function refreshProblemsListSilent() {
  const hash = window.location.hash || '#/problems';
  if (hash === '#/problems') {
    await fetchProblems();
    renderProblemsTable();
  }
}

// ==========================================================================
// Views Rendering
// ==========================================================================

// VIEW 1: Problems List
async function viewProblemsList() {
  document.getElementById('view-problem-detail').classList.add('hidden');
  document.getElementById('view-problems').classList.remove('hidden');
  document.getElementById('nav-problems').classList.add('active');
  
  // Render loading skeleton
  renderTableSkeleton();
  
  await fetchProblems();
  renderProblemsTable();
}

function renderTableSkeleton() {
  const body = document.getElementById('problemsTableBody');
  body.innerHTML = `
    <tr class="skeleton-row"><td colspan="6"><div class="skeleton"></div></td></tr>
    <tr class="skeleton-row"><td colspan="6"><div class="skeleton"></div></td></tr>
    <tr class="skeleton-row"><td colspan="6"><div class="skeleton"></div></td></tr>
  `;
}

function renderProblemsTable() {
  const body = document.getElementById('problemsTableBody');
  const noData = document.getElementById('noProblemsMessage');
  body.innerHTML = '';
  
  // Filter problems
  const filtered = problemsList.filter(p => {
    const matchCat = currentCategory === 'all' || p.category === currentCategory;
    const matchSearch = p.title.toLowerCase().includes(searchQuery.toLowerCase()) || 
                        p.id.toLowerCase().includes(searchQuery.toLowerCase());
    return matchCat && matchSearch;
  });
  
  if (filtered.length === 0) {
    noData.classList.remove('hidden');
    return;
  }
  noData.classList.add('hidden');

  const solvedStatuses = JSON.parse(localStorage.getItem('oj_solved_statuses') || '{}');
  
  const categoryNames = {
    'dp': 'Quy hoạch động',
    'binary_search': 'Chặt nhị phân',
    'greedy': 'Tham lam',
    'graph': 'Đồ thị',
    'math': 'Toán học',
    'string': 'Xử lý xâu',
    'data_structure': 'Cấu trúc dữ liệu',
    'other': 'Khác'
  };

  filtered.forEach(p => {
    const tr = document.createElement('tr');
    
    // Status Column
    const status = solvedStatuses[p.id] || 'unsolved'; // solved, failed, attempted, unsolved
    let statusHTML = '';
    if (status === 'solved') {
      statusHTML = `<span class="status-icon solved" title="Đã AC"><i data-lucide="check-circle-2"></i></span>`;
    } else if (status === 'failed') {
      statusHTML = `<span class="status-icon failed" title="Chưa vượt qua"><i data-lucide="x-circle"></i></span>`;
    } else if (status === 'attempted') {
      statusHTML = `<span class="status-icon attempted" title="Đã nộp nhưng chưa full"><i data-lucide="alert-circle"></i></span>`;
    } else {
      statusHTML = `<span class="status-icon muted" style="color: var(--text-muted);"><i data-lucide="circle"></i></span>`;
    }

    const catName = categoryNames[p.category] || p.category || 'Khác';
    const subtaskCount = p.subtask_count || 0;

    tr.innerHTML = `
      <td>${statusHTML}</td>
      <td class="problem-title-cell" onclick="navigateTo('problem', '${p.id}')">${p.title}</td>
      <td><span class="tag">${catName}</span></td>
      <td><span class="badge badge-${p.difficulty}">${p.difficulty}</span></td>
      <td>${p.cf_rating ? `<span class="badge badge-rating">${p.cf_rating}</span>` : '-'}</td>
      <td>${subtaskCount > 0 ? `${subtaskCount} subtasks` : 'Không chia'}</td>
      <td>${p.test_count} tests</td>
    `;
    
    body.appendChild(tr);
  });
  
  lucide.createIcons();
}

function filterCategory(cat) {
  currentCategory = cat;
  
  // Update active tab styles
  const tabs = document.getElementById('categoryTabs').children;
  for (let tab of tabs) {
    const onclickStr = tab.getAttribute('onclick') || '';
    if (onclickStr.includes(`'${cat}'`)) {
      tab.classList.add('active');
    } else {
      tab.classList.remove('active');
    }
  }
  
  renderProblemsTable();
}

function handleSearch() {
  searchQuery = document.getElementById('searchInput').value;
  renderProblemsTable();
}

// VIEW 2: Problem Detail
async function viewProblemDetail(id) {
  document.getElementById('view-problems').classList.add('hidden');
  document.getElementById('view-problem-detail').classList.remove('hidden');
  document.getElementById('nav-problems').classList.remove('active');
  
  // Loading state
  document.getElementById('prob-title').textContent = 'Đang tải bài tập...';
  document.getElementById('prob-description').innerHTML = '';
  document.getElementById('prob-input-format').innerHTML = '';
  document.getElementById('prob-output-format').innerHTML = '';
  document.getElementById('prob-constraints').innerHTML = '';
  document.getElementById('prob-subtasks').innerHTML = '';
  document.getElementById('prob-samples').innerHTML = '';
  document.getElementById('quickResultBox').classList.add('hidden');
  
  currentProblem = await fetchProblemDetail(id);
  if (!currentProblem) {
    navigateTo('problems');
    return;
  }
  
  // Fill details
  document.getElementById('prob-title').textContent = currentProblem.title;
  
  const diffBadge = document.getElementById('prob-difficulty');
  diffBadge.textContent = currentProblem.difficulty;
  diffBadge.className = `meta-pill badge-${currentProblem.difficulty}`;

  const ratingEl = document.getElementById('prob-cf-rating');
  if (currentProblem.cf_rating) {
    ratingEl.textContent = `Rating: ${currentProblem.cf_rating}`;
    ratingEl.style.display = '';
  } else {
    ratingEl.style.display = 'none';
  }
  
  const catNames = { 'dp': 'Quy hoạch động', 'binary_search': 'Chặt nhị phân', 'greedy': 'Tham lam', 'graph': 'Đồ thị', 'math': 'Toán học', 'string': 'Xử lý xâu', 'data_structure': 'Cấu trúc dữ liệu' };
  document.getElementById('prob-category').textContent = catNames[currentProblem.category] || currentProblem.category || 'Khác';
  document.getElementById('prob-time-limit').textContent = `${currentProblem.time_limit}s`;
  document.getElementById('prob-memory-limit').textContent = `${currentProblem.memory_limit || 256}MB`;
  
  document.getElementById('prob-description').innerHTML = formatRichText(currentProblem.description);
  
  // Formats sections
  fillOrHideSection('section-input-format', 'prob-input-format', currentProblem.input_format);
  fillOrHideSection('section-output-format', 'prob-output-format', currentProblem.output_format);
  fillOrHideSection('section-constraints', 'prob-constraints', currentProblem.constraints);
  
  // Subtasks
  const subtasksContainer = document.getElementById('prob-subtasks');
  const sectionSubtasks = document.getElementById('section-subtasks');
  if (currentProblem.subtasks && currentProblem.subtasks.length > 0) {
    sectionSubtasks.classList.remove('hidden');
    subtasksContainer.innerHTML = '';
    currentProblem.subtasks.forEach(sub => {
      const div = document.createElement('div');
      div.className = 'subtask-item';
      div.innerHTML = `
        <div class="subtask-info">
          <span class="subtask-name">Subtask #${sub.id}</span>
          <span class="subtask-constraints">${sub.constraints || 'Không có ràng buộc phụ'}</span>
        </div>
        <span class="subtask-points">${sub.points} điểm</span>
      `;
      subtasksContainer.appendChild(div);
    });
  } else {
    sectionSubtasks.classList.add('hidden');
  }

  // Samples
  const samplesContainer = document.getElementById('prob-samples');
  if (currentProblem.samples && currentProblem.samples.length > 0) {
    samplesContainer.innerHTML = '';
    currentProblem.samples.forEach((sample, index) => {
      const block = document.createElement('div');
      block.className = 'sample-block';
      
      let explanationHTML = '';
      if (sample.explanation) {
        explanationHTML = `<div class="sample-explanation"><strong>Giải thích:</strong> ${sample.explanation}</div>`;
      }

      block.innerHTML = `
        <div class="sample-header">
          <span>Ví dụ #${index + 1}</span>
          <button onclick="copyText(this, ${index})" class="copy-btn"><i data-lucide="copy" style="width:14px;height:14px;"></i> Sao chép</button>
        </div>
        <pre class="sample-body" id="sample-input-${index}">${sample.input}</pre>
        <div class="sample-header" style="border-top: 1px solid var(--border-color)">
          <span>Kết quả mẫu</span>
        </div>
        <pre class="sample-body">${sample.output}</pre>
        ${explanationHTML}
      `;
      samplesContainer.appendChild(block);
    });
  } else {
    samplesContainer.innerHTML = '<p class="text-secondary">Không có ví dụ mẫu.</p>';
  }

  // Load editor code draft
  const drafts = JSON.parse(localStorage.getItem('oj_editor_drafts') || '{}');
  const codeEditor = document.getElementById('codeEditor');
  codeEditor.value = drafts[currentProblem.id] || '';
  
  // Save draft on edit
  codeEditor.oninput = () => {
    drafts[currentProblem.id] = codeEditor.value;
    localStorage.setItem('oj_editor_drafts', JSON.stringify(drafts));
  };
  
  lucide.createIcons();
}

function fillOrHideSection(sectionId, elementId, value) {
  const section = document.getElementById(sectionId);
  const element = document.getElementById(elementId);
  if (value && value.trim()) {
    section.classList.remove('hidden');
    element.innerHTML = formatRichText(value);
  } else {
    section.classList.add('hidden');
  }
}

// ---------- Solution Help ----------
async function requestSolutionHint() {
  requestSolution('hint', 'Hướng dẫn giải');
}

async function requestSolutionCode() {
  requestSolution('code', 'Code giải');
}

async function requestSolution(type, title) {
  if (!currentProblem) {
    showToast('Vui lòng chọn bài tập trước.', 'error');
    return;
  }
  // Read config from saved settings
  var cfg = { aiKey: '', aiProvider: 'openai' };
  var raw = localStorage.getItem('oj_local_settings');
  if (raw) { try { cfg = JSON.parse(raw); } catch(e) {} }
  var apiKey = cfg.aiKey || '';
  var provider = cfg.aiProvider || 'openai';
  if (!apiKey) {
    showToast('Vui lòng nhập API key trong Settings > Cấu hình AI.', 'error');
    return;
  }

  const resultBox = document.getElementById('solution-result');
  const resultTitle = document.getElementById('solution-result-title');
  const resultBody = document.getElementById('solution-result-body');
  resultTitle.textContent = 'Đang tải...';
  resultBody.textContent = '';
  resultBox.classList.remove('hidden');
  if (typeof lucide !== 'undefined') lucide.createIcons();

  try {
    const resp = await fetch(API_BASE + '/api/solution/' + type, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ problem_id: currentProblem.id, api_key: apiKey, provider: provider })
    });
    const data = await resp.json();

    resultTitle.textContent = title;
    if (data.error) {
      resultBody.innerHTML = '<span style="color:var(--danger)">' + data.error + '</span>';
    } else if (type === 'hint') {
      let html = '';
      if (data.approach) html += '<p><strong>Cách tiếp cận:</strong></p><p>' + formatRichText(data.approach) + '</p>';
      if (data.algorithm) html += '<p><strong>Thuật toán:</strong> ' + formatRichText(data.algorithm) + '</p>';
      if (data.complexity) html += '<p><strong>Độ phức tạp:</strong> ' + data.complexity + '</p>';
      if (data.key_insight) html += '<p><strong>Mấu chốt:</strong> ' + formatRichText(data.key_insight) + '</p>';
      if (data.steps && Array.isArray(data.steps)) {
        html += '<p><strong>Các bước:</strong></p><ol>';
        data.steps.forEach(function(s) { html += '<li>' + formatRichText(s) + '</li>'; });
        html += '</ol>';
      }
      resultBody.innerHTML = html || formatRichText(JSON.stringify(data, null, 2));
    } else if (type === 'code') {
      if (data.code) {
        resultBody.innerHTML = '<pre class="code-solution">' + data.code.replace(/</g, '&lt;').replace(/>/g, '&gt;') + '</pre>';
        if (data.explanation) {
          resultBody.innerHTML += '<p><strong>Giải thích:</strong> ' + formatRichText(data.explanation) + '</p>';
        }
        if (data.complexity) {
          resultBody.innerHTML += '<p><strong>Độ phức tạp:</strong> ' + data.complexity + '</p>';
        }
      } else {
        resultBody.innerHTML = formatRichText(JSON.stringify(data, null, 2));
      }
    }
    if (typeof lucide !== 'undefined') lucide.createIcons();
  } catch (e) {
    resultTitle.textContent = 'Lỗi';
    resultBody.innerHTML = '<span style="color:var(--danger)">Lỗi kết nối: ' + e.message + '</span>';
  }
}

function closeSolutionResult() {
  document.getElementById('solution-result').classList.add('hidden');
}

function formatRichText(text) {
  if (!text) return '';
  // Shared math transformations (safe to apply on plain text, no HTML entities involved)
  function applyMath(s) {
    s = s.replace(/\^\{([^}]+)\}/g, '<sup>$1</sup>');
    s = s.replace(/\^(\w+)/g, '<sup>$1</sup>');
    s = s.replace(/_\{([^}]+)\}/g, '<sub>$1</sub>');
    s = s.replace(/_(\w+)/g, '<sub>$1</sub>');
    s = s.replace(/\\le/g, '≤').replace(/\\ge/g, '≥');
    s = s.replace(/\\neq/g, '≠').replace(/\\cdot/g, '·');
    s = s.replace(/\\times/g, '×').replace(/\\infty/g, '∞');
    s = s.replace(/\\sqrt\{([^}]+)\}/g, '√$1');
    s = s.replace(/[{}]/g, '');
    return s;
  }
  // Split by $...$ to isolate math sections
  var parts = text.split(/(\$[^$]+\$)/);
  for (var i = 0; i < parts.length; i++) {
    var isMath = parts[i].charAt(0) === '$' && parts[i].charAt(parts[i].length-1) === '$';
    if (isMath) {
      var inner = applyMath(parts[i].slice(1, -1));
      parts[i] = '<span class="math-inline">' + inner + '</span>';
    } else {
      // For non-math: escape HTML first, then apply math (safe because ^ _ \ {} are not HTML special)
      var s = parts[i].replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
      parts[i] = applyMath(s);
    }
  }
  return parts.join('').replace(/\n/g, '<br>');
}

// Copy Sample input helper
function copyText(btn, index) {
  const pre = document.getElementById(`sample-input-${index}`);
  navigator.clipboard.writeText(pre.textContent).then(() => {
    btn.innerHTML = `<i data-lucide="check" style="width:14px;height:14px;color:var(--success)"></i> Đã chép`;
    lucide.createIcons();
    setTimeout(() => {
      btn.innerHTML = `<i data-lucide="copy" style="width:14px;height:14px;"></i> Sao chép`;
      lucide.createIcons();
    }, 2000);
  });
}

// ==========================================================================
// Code Submit & Judging Flow
// ==========================================================================
async function handleCodeSubmit() {
  if (!currentProblem) return;
  const code = document.getElementById('codeEditor').value;
  if (!code.trim()) {
    showToast('Vui lòng viết mã nguồn trước khi nộp bài!', 'error');
    return;
  }

  const btn = document.getElementById('btnSubmit');
  btn.disabled = true;
  btn.innerHTML = `<i data-lucide="loader" class="spinning"></i> Đang chấm...`;
  lucide.createIcons();
  
  const resultBox = document.getElementById('quickResultBox');
  resultBox.classList.remove('hidden');
  
  const verdictText = document.getElementById('quickVerdict');
  verdictText.className = 'verdict-text JUDGING';
  verdictText.textContent = 'Judging...';
  
  const scoreVal = document.getElementById('quickScore');
  scoreVal.textContent = '- / -';
  document.getElementById('quickScoreProgress').style.width = '0%';
  document.getElementById('quickSubtaskResults').innerHTML = '';

  try {
    const resp = await fetch(API_BASE + '/api/judge', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        problem_id: currentProblem.id,
        code: code,
        language: 'cpp',
        compiler_path: (() => { try { const s = JSON.parse(localStorage.getItem('oj_local_settings') || '{}'); return s.compilerPath || ''; } catch(e) { return ''; } })()
      })
    });
    
    if (!resp.ok) throw new Error('Không thể kết nối đến judge engine');
    
    lastJudgeResult = await resp.json();
    displayQuickResult(lastJudgeResult);
    saveLocalProblemStatus(currentProblem.id, lastJudgeResult);
    
  } catch (e) {
    showToast(e.message, 'error');
    verdictText.className = 'verdict-text WA';
    verdictText.textContent = 'Chấm lỗi';
  } finally {
    btn.disabled = false;
    btn.innerHTML = `<i data-lucide="send"></i> Nộp bài (Ctrl+Enter)`;
    lucide.createIcons();
  }
}

function displayQuickResult(res) {
  const verdictText = document.getElementById('quickVerdict');
  verdictText.textContent = getVerdictString(res.verdict);
  verdictText.className = `verdict-text ${res.verdict}`;
  
  const scoreVal = document.getElementById('quickScore');
  const points = res.score !== undefined ? res.score : (res.verdict === 'AC' ? 100 : 0);
  const maxPoints = res.max_score !== undefined ? res.max_score : 100;
  
  scoreVal.textContent = `${points} / ${maxPoints}`;
  
  const pct = maxPoints > 0 ? (points / maxPoints) * 100 : 0;
  document.getElementById('quickScoreProgress').style.width = `${pct}%`;

  // Render subtask badges
  const subContainer = document.getElementById('quickSubtaskResults');
  subContainer.innerHTML = '';

  if (res.subtask_results && res.subtask_results.length > 0) {
    res.subtask_results.forEach(sub => {
      const badge = document.createElement('div');
      badge.className = `subtask-badge-mini ${sub.status}`;
      badge.innerHTML = `Sub ${sub.id}<br>${sub.points}/${sub.max_points}`;
      subContainer.appendChild(badge);
    });
  } else {
    // If no subtasks were defined, render single test status info
    const div = document.createElement('div');
    div.style.gridColumn = '1 / -1';
    div.style.fontSize = '0.8em';
    div.style.color = 'var(--text-secondary)';
    div.textContent = `Đúng ${res.passed} trên tổng số ${res.total} testcases.`;
    subContainer.appendChild(div);
  }
}

function saveLocalProblemStatus(probId, res) {
  const solvedStatuses = JSON.parse(localStorage.getItem('oj_solved_statuses') || '{}');
  
  let status = 'failed';
  if (res.verdict === 'AC') {
    status = 'solved';
  } else if (res.score > 0) {
    status = 'attempted';
  }
  
  solvedStatuses[probId] = status;
  localStorage.setItem('oj_solved_statuses', JSON.stringify(solvedStatuses));
}

function getVerdictString(verdict) {
  const verdicts = {
    'AC': 'Accepted',
    'WA': 'Wrong Answer',
    'TLE': 'Time Limit Exceeded',
    'RE': 'Runtime Error',
    'CE': 'Compile Error'
  };
  return verdicts[verdict] || verdict;
}

// ==========================================================================
// Detailed Result Modal Flow
// ==========================================================================
function showDetailedResultModal() {
  if (!lastJudgeResult) return;
  const res = lastJudgeResult;
  
  document.getElementById('modal-results').classList.remove('hidden');
  document.getElementById('modal-result-subtitle').textContent = `Bài tập: ${currentProblem ? currentProblem.title : 'N/A'}`;
  
  // Large Verdict Badge
  const badgeLarge = document.getElementById('modalVerdictLarge');
  badgeLarge.textContent = getVerdictString(res.verdict);
  badgeLarge.className = `verdict-badge-large verdict-text ${res.verdict}`;
  
  // Points
  const points = res.score !== undefined ? res.score : (res.verdict === 'AC' ? 100 : 0);
  const maxPoints = res.max_score !== undefined ? res.max_score : 100;
  document.getElementById('modalPoints').textContent = `${points} / ${maxPoints}`;

  // Compile Error
  const ceContainer = document.getElementById('compileErrorContainer');
  if (res.verdict === 'CE') {
    ceContainer.classList.remove('hidden');
    document.getElementById('compileErrorText').textContent = res.compile_error || 'No compilation output returned.';
  } else {
    ceContainer.classList.add('hidden');
  }

  // Subtasks Table Body
  const subBody = document.getElementById('modalSubtaskTableBody');
  subBody.innerHTML = '';
  
  if (res.subtask_results && res.subtask_results.length > 0) {
    res.subtask_results.forEach(sub => {
      const subInfo = currentProblem?.subtasks?.find(s => s.id === sub.id) || {};
      const tr = document.createElement('tr');
      tr.innerHTML = `
        <td><strong>Subtask #${sub.id}</strong></td>
        <td>${sub.points} / ${sub.max_points}</td>
        <td><span class="text-secondary">${subInfo.constraints || 'Không có'}</span></td>
        <td><span class="verdict-text ${sub.status}" style="font-size:0.9em;">${getVerdictString(sub.status)}</span></td>
        <td>${sub.passed} / ${sub.total}</td>
      `;
      subBody.appendChild(tr);
    });
  } else {
    // default
    const tr = document.createElement('tr');
    tr.innerHTML = `
      <td><strong>Full Tests</strong></td>
      <td>${points} / ${maxPoints}</td>
      <td><span>Đầy đủ ràng buộc</span></td>
      <td><span class="verdict-text ${res.verdict}" style="font-size:0.9em;">${getVerdictString(res.verdict)}</span></td>
      <td>${res.passed} / ${res.total}</td>
    `;
    subBody.appendChild(tr);
  }

  // Detailed Testcases render
  openTestcaseIndex = null; // reset collapse
  renderTestcasesList();
}

function renderTestcasesList() {
  const container = document.getElementById('modalTestcaseList');
  container.innerHTML = '';
  
  if (!lastJudgeResult || !lastJudgeResult.test_cases) {
    container.innerHTML = '<p class="text-muted">Không có thông tin testcase.</p>';
    return;
  }
  
  lastJudgeResult.test_cases.forEach((tc, idx) => {
    const item = document.createElement('div');
    item.className = 'testcase-detail-item';
    
    const isExpanded = openTestcaseIndex === idx;
    const expandIcon = isExpanded ? 'chevron-up' : 'chevron-down';
    
    // Header row
    const header = document.createElement('div');
    header.className = 'testcase-summary-bar';
    header.onclick = () => toggleTestcaseDetail(idx);
    
    const subtaskText = tc.subtask ? ` | Subtask #${tc.subtask}` : '';
    
    header.innerHTML = `
      <div class="testcase-meta">
        <span class="testcase-idx">Test #${tc.index}${subtaskText}</span>
        <span class="testcase-badge ${tc.status}">${tc.status}</span>
        <span class="testcase-time"><i data-lucide="clock" style="width:12px;height:12px;display:inline;vertical-align:middle;"></i> ${tc.time.toFixed(3)}s</span>
      </div>
      <i data-lucide="${expandIcon}"></i>
    `;
    item.appendChild(header);
    
    // Details expanded area
    if (isExpanded) {
      const detail = document.createElement('div');
      detail.className = 'testcase-expand-area';
      
      let errorBlock = '';
      if (tc.error) {
        errorBlock = `<div class="error-msg" style="color:var(--danger);font-size:0.85em;"><strong>Lỗi:</strong> ${tc.error}</div>`;
      }

      // Slice large strings to prevent freezing UI
      const inputStr = limitStringLength(tc.input, 2000);
      const expectedStr = limitStringLength(tc.expected, 2000);
      const gotStr = limitStringLength(tc.output, 2000);

      detail.innerHTML = `
        ${errorBlock}
        <div class="code-comparison-grid">
          <div class="comparison-box">
            <span>Input</span>
            <pre>${inputStr}</pre>
          </div>
          <div class="comparison-box">
            <span>Expected Output</span>
            <pre>${expectedStr}</pre>
          </div>
        </div>
        <div class="code-comparison-grid" style="margin-top: 8px;">
          <div class="comparison-box" style="grid-column: 1 / -1">
            <span>Your Code Output</span>
            <pre style="background-color: var(--bg-secondary); color:${tc.status === 'AC' ? 'var(--success)' : 'var(--danger)'}">${gotStr}</pre>
          </div>
        </div>
      `;
      item.appendChild(detail);
    }
    
    container.appendChild(item);
  });
  
  lucide.createIcons();
}

function toggleTestcaseDetail(idx) {
  if (openTestcaseIndex === idx) {
    openTestcaseIndex = null;
  } else {
    openTestcaseIndex = idx;
  }
  renderTestcasesList();
}

function limitStringLength(str, maxLen) {
  if (!str) return '';
  if (str.length > maxLen) {
    return str.substring(0, maxLen) + '\n... [Bị cắt bớt do dữ liệu quá lớn]';
  }
  return str;
}

function closeDetailedResultModal() {
  document.getElementById('modal-results').classList.add('hidden');
}

// ==========================================================================
// Settings Modal Flow
// ==========================================================================
function openSettings() {
  populateSettingsAIModels();
  loadSettingsToUI();
  document.getElementById('modal-settings').classList.remove('hidden');
  if (typeof lucide !== 'undefined') lucide.createIcons();
}

function closeSettings() {
  document.getElementById('modal-settings').classList.add('hidden');
}

function saveSettings() {
  const data = {
    editorSize: document.getElementById('settings-editor-size').value,
    tabSize: document.getElementById('settings-tab-size').value,
    compilerPath: document.getElementById('settings-compiler-path').value.trim(),
    apiBase: document.getElementById('settings-api-url').value.trim(),
    aiProvider: document.getElementById('settings-ai-provider').value,
    aiModel: document.getElementById('settings-ai-model').value,
    aiKey: document.getElementById('settings-ai-key').value
  };
  localStorage.setItem('oj_local_settings', JSON.stringify(data));
}

function loadSettingsToUI() {
  const raw = localStorage.getItem('oj_local_settings');
  if (raw) {
    const s = JSON.parse(raw);
    document.getElementById('settings-editor-size').value = s.editorSize || '14px';
    document.getElementById('settings-tab-size').value = s.tabSize || '4';
    document.getElementById('settings-compiler-path').value = s.compilerPath || '';
    document.getElementById('settings-api-url').value = s.apiBase || '';
    if (s.aiProvider) document.getElementById('settings-ai-provider').value = s.aiProvider;
    populateSettingsAIModels(s.aiModel);
    if (s.aiKey) document.getElementById('settings-ai-key').value = s.aiKey;
  } else {
    populateSettingsAIModels();
  }
}

function populateSettingsAIModels(selectedModel) {
  const provider = document.getElementById('settings-ai-provider').value;
  const sel = document.getElementById('settings-ai-model');
  sel.innerHTML = '';
  const models = GEN_MODELS[provider] || [];
  models.forEach(function(m) {
    var opt = document.createElement('option');
    opt.value = m.value;
    opt.textContent = m.label;
    if (m.value === selectedModel) opt.selected = true;
    sel.appendChild(opt);
  });
}

function applySettings() {
  const size = document.getElementById('settings-editor-size').value;
  const tab = document.getElementById('settings-tab-size').value;
  const compilerPath = document.getElementById('settings-compiler-path').value.trim();
  const apiUrl = document.getElementById('settings-api-url').value.trim();
  
  const editor = document.getElementById('codeEditor');
  editor.style.fontSize = size;
  editor.style.tabSize = tab;
  
  const apiVal = apiUrl || '';
  localStorage.setItem('api_base', apiVal);
  API_BASE = apiVal;
  
  saveSettings();
}

function loadSettings() {
  loadSettingsToUI();
  applySettings();
}

// ==========================================================================
// Toast System
// ==========================================================================
function showToast(message, type = 'info') {
  const container = document.getElementById('toastContainer');
  const toast = document.createElement('div');
  toast.className = `toast toast-${type}`;
  
  let iconName = 'info';
  if (type === 'success') iconName = 'check-circle-2';
  if (type === 'error') iconName = 'alert-triangle';
  
  toast.innerHTML = `
    <i data-lucide="${iconName}"></i>
    <span>${message}</span>
  `;
  
  container.appendChild(toast);
  lucide.createIcons();
  
  // Slide out after 3.5s
  setTimeout(() => {
    toast.style.opacity = '0';
    toast.style.transform = 'translateY(8px)';
    setTimeout(() => toast.remove(), 200);
  }, 3500);
}

// ==========================================================================
// Editor Helpers & Shortcuts
// ==========================================================================
function clearEditor() {
  if (confirm('Bạn có chắc chắn muốn xoá toàn bộ mã nguồn hiện tại?')) {
    document.getElementById('codeEditor').value = '';
    // trigger input save draft
    document.getElementById('codeEditor').dispatchEvent(new Event('input'));
  }
}

function copyFromClipboard() {
  navigator.clipboard.readText().then(text => {
    const editor = document.getElementById('codeEditor');
    editor.value = text;
    editor.dispatchEvent(new Event('input'));
    showToast('Đã dán code thành công!', 'success');
  }).catch(err => {
    showToast('Lỗi: Trình duyệt chặn quyền truy cập Clipboard', 'error');
  });
}

function setupEditorKeyboardShortcuts() {
  const editor = document.getElementById('codeEditor');
  
  editor.addEventListener('keydown', e => {
    // 1. Tab Support
    if (e.key === 'Tab') {
      e.preventDefault();
      const tabSize = parseInt(document.getElementById('settings-tab-size').value) || 4;
      const spaces = ' '.repeat(tabSize);
      
      const start = editor.selectionStart;
      const end = editor.selectionEnd;
      
      editor.value = editor.value.substring(0, start) + spaces + editor.value.substring(end);
      editor.selectionStart = editor.selectionEnd = start + tabSize;
      
      // trigger draft auto-save
      editor.dispatchEvent(new Event('input'));
    }
    
    // 2. Ctrl + Enter to submit
    if (e.ctrlKey && e.key === 'Enter') {
      e.preventDefault();
      handleCodeSubmit();
    }
  });
}

// ==========================================================================
// AI Problem Generation
// ==========================================================================

const GEN_CATEGORIES = {
  'toan': {
    name: 'Toán học',
    subTypes: ['Số học cơ bản', 'Số nguyên tố', 'GCD/LCM', 'Tổ hợp', 'Số học modulo', 'Dãy số', 'Phương trình']
  },
  'chat_nhi_phan': {
    name: 'Chặt nhị phân',
    subTypes: ['Tìm kiếm cơ bản', 'Căn bậc hai', 'Tìm kiếm trên mảng', 'Chặt nhị phân đáp án', 'Ternary search']
  },
  'dp': {
    name: 'Quy hoạch động',
    subTypes: ['DP cơ bản', 'DP dãy con (LIS/LCS)', 'DP số (Digit DP)', 'DP on tree', 'DP thứ tự từ điển', 'DP + CTDL', 'DP xác suất', 'DP cửa sổ']
  },
  'tham_lam': {
    name: 'Tham lam',
    subTypes: ['Sắp xếp', 'Hoán vị', 'Tối ưu']
  },
  'do_thi': {
    name: 'Đồ thị',
    subTypes: ['BFS/DFS', 'Dijkstra', 'Floyd-Warshall', 'Bellman-Ford', 'Kruskal/Prim (MST)', 'Topological sort', 'Euler path', 'Flow']
  },
  'cau_truc_du_lieu': {
    name: 'Cấu trúc dữ liệu',
    subTypes: ['Stack/Queue', 'Segment Tree', 'Fenwick Tree (BIT)', 'DSU', 'Trie', 'Heap/Priority Queue']
  },
  'xau_ky_tu': {
    name: 'Xử lý xâu',
    subTypes: ['Xâu đối xứng', 'Tìm kiếm xâu (KMP)', 'Z-algorithm', 'Hash']
  },
  'khac': {
    name: 'Khác',
    subTypes: ['Two pointers', 'Sliding window', 'Prefix sum', 'Sorting', 'Simulation']
  }
};

const GEN_MODELS = {
  'openai': [
    { value: 'gpt-4o', label: 'GPT-4o' },
    { value: 'gpt-4o-mini', label: 'GPT-4o Mini' },
    { value: 'gpt-3.5-turbo', label: 'GPT-3.5 Turbo' }
  ],
  'claude': [
    { value: 'claude-opus-4-8', label: 'Opus 4.8' },
    { value: 'claude-fable-5', label: 'Fable 5' },
    { value: 'claude-sonnet-4-6', label: 'Sonnet 4.6' },
    { value: 'claude-haiku-4-5-20251001', label: 'Haiku 4.5' }
  ],
  'gemini': [
    { value: 'gemini-3.5-flash', label: 'Gemini 3.5 Flash' },
    { value: 'gemini-3.1-pro-preview', label: 'Gemini 3.1 Pro' },
    { value: 'gemini-3.1-flash-lite', label: 'Gemini 3.1 Flash Lite' }
  ],
  'deepseek': [
    { value: 'deepseek-chat', label: 'DeepSeek Chat' },
    { value: 'deepseek-coder', label: 'DeepSeek Coder' }
  ]
};

function loadGenerateConfig() {
  var raw = localStorage.getItem('oj_local_settings');
  if (!raw) return;
  var s = JSON.parse(raw);
  if (s.aiProvider) document.getElementById('gen-provider').value = s.aiProvider;
  if (s.aiKey) document.getElementById('gen-api-key').value = s.aiKey;
  onProviderChange(s.aiModel);
}

function saveGenerateConfig() {
  var raw = localStorage.getItem('oj_local_settings');
  var s = raw ? JSON.parse(raw) : {};
  s.aiProvider = document.getElementById('gen-provider').value;
  s.aiModel = document.getElementById('gen-model').value;
  s.aiKey = document.getElementById('gen-api-key').value;
  localStorage.setItem('oj_local_settings', JSON.stringify(s));
}

function openGenerateModal() {
  document.getElementById('modal-generate').classList.remove('hidden');
  document.getElementById('gen-result-area').classList.add('hidden');
  document.getElementById('gen-result-loading').classList.add('hidden');
  document.getElementById('gen-result-success').classList.add('hidden');
  document.getElementById('gen-result-error').classList.add('hidden');
  document.getElementById('btnGenSubmit').disabled = false;
  document.getElementById('btnGenSubmit').innerHTML = '<i data-lucide="wand-2"></i> Sinh đề';
  loadGenerateConfig();
  lucide.createIcons();
}

function closeGenerateModal() {
  document.getElementById('modal-generate').classList.add('hidden');
}

function onProviderChange(selectedModel) {
  const provider = document.getElementById('gen-provider').value;
  const input = document.getElementById('gen-api-key');
  const modelSelect = document.getElementById('gen-model');
  
  // Update placeholder
  if (provider === 'openai') input.placeholder = 'sk-...';
  else if (provider === 'claude') input.placeholder = 'sk-ant-...';
  else if (provider === 'gemini') input.placeholder = 'AIzaSy...';
  else if (provider === 'deepseek') input.placeholder = 'sk-...';
  
  // Update model dropdown
  modelSelect.innerHTML = '';
  const models = GEN_MODELS[provider] || [];
  models.forEach(m => {
    const opt = document.createElement('option');
    opt.value = m.value;
    opt.textContent = m.label;
    if (selectedModel && m.value === selectedModel) opt.selected = true;
    modelSelect.appendChild(opt);
  });
  saveGenerateConfig();
}

function updateGenSubTypes() {
  const cat = document.getElementById('gen-category').value;
  const subSelect = document.getElementById('gen-sub-type');
  subSelect.innerHTML = '';
  if (!cat || !GEN_CATEGORIES[cat]) {
    subSelect.innerHTML = '<option value="">-- Chọn thể loại trước --</option>';
    return;
  }
  const types = GEN_CATEGORIES[cat].subTypes;
  types.forEach(t => {
    const opt = document.createElement('option');
    opt.value = t;
    opt.textContent = t;
    subSelect.appendChild(opt);
  });
  saveGenerateConfig();
}

async function handleGenerateProblem() {
  const provider = document.getElementById('gen-provider').value;
  const apiKey = document.getElementById('gen-api-key').value.trim();
  const category = document.getElementById('gen-category').value;
  const subType = document.getElementById('gen-sub-type').value;
  const subtaskCount = parseInt(document.getElementById('gen-subtask-count').value);

  if (!apiKey) {
    showToast('Vui lòng nhập API key!', 'error');
    return;
  }
  if (!category) {
    showToast('Vui lòng chọn thể loại!', 'error');
    return;
  }
  if (!subType) {
    showToast('Vui lòng chọn dạng bài!', 'error');
    return;
  }

  const btn = document.getElementById('btnGenSubmit');
  btn.disabled = true;
  btn.innerHTML = '<i data-lucide="loader" class="spinning"></i> Đang sinh...';

  const resultArea = document.getElementById('gen-result-area');
  resultArea.classList.remove('hidden');
  document.getElementById('gen-result-loading').classList.remove('hidden');
  document.getElementById('gen-result-success').classList.add('hidden');
  document.getElementById('gen-result-error').classList.add('hidden');
  lucide.createIcons();

  const model = document.getElementById('gen-model').value;
  const rating = document.getElementById('gen-rating').value;

  try {
    const resp = await fetch(API_BASE + '/api/generate', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        provider: provider,
        model: model,
        api_key: apiKey,
        category: category,
        sub_type: subType,
        subtask_count: subtaskCount,
        rating: rating
      })
    });

    const data = await resp.json();

    document.getElementById('gen-result-loading').classList.add('hidden');

    if (data.error) {
      document.getElementById('gen-result-error').classList.remove('hidden');
      document.getElementById('gen-result-error-text').textContent = data.error;
      showToast('Sinh đề thất bại: ' + data.error, 'error');
    } else {
      document.getElementById('gen-result-success').classList.remove('hidden');
      document.getElementById('gen-result-title').innerHTML = '<strong>Bài tập:</strong> ' + data.title;
      document.getElementById('gen-result-meta').textContent =
        'ID: ' + data.problem_id + ' | ' + data.test_count + ' test cases | ' + data.subtask_count + ' subtasks';
      document.getElementById('gen-result-link').setAttribute('data-id', data.problem_id);
      showToast('Sinh đề thành công: ' + data.title, 'success');
    }
  } catch (e) {
    document.getElementById('gen-result-loading').classList.add('hidden');
    document.getElementById('gen-result-error').classList.remove('hidden');
    document.getElementById('gen-result-error-text').textContent = e.message;
    showToast('Lỗi kết nối: ' + e.message, 'error');
  } finally {
    btn.disabled = false;
    btn.innerHTML = '<i data-lucide="wand-2"></i> Sinh đề';
    lucide.createIcons();
  }
}

function openGeneratedProblem(event) {
  event.preventDefault();
  const id = event.currentTarget.getAttribute('data-id');
  if (id) {
    closeGenerateModal();
    navigateTo('problem', id);
  }
}
