// TEGE - loading screen
(function() {
  'use strict';

  var progressBar = document.getElementById('progress-bar');
  var progressText = document.getElementById('progress-text');
  var preloader = document.getElementById('preloader');
  var clickToPlay = document.getElementById('click-to-play');
  var cube = document.getElementById('tege-cube');

  var statusLines = [
    'loading engine',
    'loading assets',
    'preparing shaders',
    'starting up',
    'almost ready'
  ];

  var shown = 0;       // eased display value
  var target = 0.08;   // creeps while loading, 1.0 when the engine is up
  var ready = false;

  window.updateProgress = function(ratio) {
    ratio = Math.max(0, Math.min(1, ratio));
    if (ratio > target) target = ratio;
  };

  var tick = setInterval(function() {
    // Creep toward the target, but hold at 90% until the engine is
    // actually initialized (no fake done-then-nothing bars)
    if (!ready && target < 0.9) target += 0.004;
    var cap = ready ? 1.0 : 0.9;
    shown += (Math.min(target, cap) - shown) * 0.12;
    var pct = shown * 100;
    if (progressBar) progressBar.style.width = pct + '%';
    if (cube) cube.style.left = pct + '%';
    if (progressText) {
      var idx = Math.min(statusLines.length - 1, Math.floor(shown * statusLines.length));
      progressText.textContent = statusLines[idx];
    }
    if (ready && shown > 0.995) {
      clearInterval(tick);
      showClickToPlay();
    }
  }, 33);

  function showClickToPlay() {
    if (progressText) progressText.style.display = 'none';
    if (document.getElementById('progress-container'))
      document.getElementById('progress-container').style.display = 'none';
    if (clickToPlay) {
      clickToPlay.style.display = 'block';
      clickToPlay.addEventListener('click', function() {
        // Resume audio context (needs a user gesture)
        if (typeof Module !== 'undefined' && Module._resumeAudio)
          Module._resumeAudio();
        preloader.style.transition = 'opacity 0.5s ease';
        preloader.style.opacity = '0';
        setTimeout(function() {
          preloader.style.display = 'none';
          document.getElementById('game-canvas').focus();
        }, 500);
      });
    }
  }

  // Called by the Module when the engine finishes initializing
  window.hidePreloader = function() {
    if (!preloader) return;
    ready = true;
    target = 1.0;
  };
})();
