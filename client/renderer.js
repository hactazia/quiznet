// QuizNet Client - Renderer Process

class QuizNetClient {
    constructor() {
        this.currentScreen = 'connection-screen';
        this.pseudo = '';
        this.isCreator = false;
        this.sessionId = null;
        this.sessionMode = 'solo';
        this.themes = [];
        this.score = 0;
        this.lives = 4;
        this.jokers = { fifty: 1, skip: 1 };
        this.timerInterval = null;
        this.currentTime = 0;
        this.questionStartTime = 0;
        this.hasAnswered = false;
        this.removedAnswers = [];
        this.currentQuestion = null;

        this.init();
    }

    init() {
        this.setupEventListeners();
        this.setupServerListeners();
        this.discoverServers();
    }

    setupEventListeners() {
        document.getElementById('refresh-servers').addEventListener('click', () => this.discoverServers());
        document.getElementById('connect-manual').addEventListener('click', () => this.connectManual());
        document.querySelectorAll('.tab').forEach(tab => tab.addEventListener('click', (e) => this.switchTab(e.target.dataset.tab)));
        document.getElementById('login-btn').addEventListener('click', () => this.login());
        document.getElementById('register-btn').addEventListener('click', () => this.register());
        document.getElementById('disconnect-btn').addEventListener('click', () => this.disconnectServer());
        document.getElementById('logout-btn').addEventListener('click', () => this.logout());
        document.getElementById('refresh-sessions').addEventListener('click', () => this.refreshSessions());
        document.getElementById('create-session').addEventListener('click', () => this.createSession());
        document.getElementById('session-mode').addEventListener('change', (e) => {
            document.getElementById('lives-option').classList.toggle('hidden', e.target.value !== 'battle');
        });
        document.getElementById('start-session').addEventListener('click', () => this.startSession());
        document.getElementById('use-fifty').addEventListener('click', () => this.useJoker('fifty'));
        document.getElementById('use-skip').addEventListener('click', () => this.useJoker('skip'));
        document.getElementById('submit-text-answer').addEventListener('click', () => this.submitTextAnswer());
        document.getElementById('back-to-lobby').addEventListener('click', () => this.backToLobby());
        document.getElementById('login-password').addEventListener('keypress', (e) => {
            if (e.key === 'Enter') this.login();
        });
        document.getElementById('register-password').addEventListener('keypress', (e) => {
            if (e.key === 'Enter') this.register();
        });
        document.getElementById('text-answer').addEventListener('keypress', (e) => {
            if (e.key === 'Enter') this.submitTextAnswer();
        });
    }

    setupServerListeners() {
        window.quiznet.onServerMessage((data) => this.handleServerMessage(data));
        window.quiznet.onConnectionError((error) => this.handleConnectionError(error));
        window.quiznet.onConnectionClosed(() => this.handleConnectionClosed());
    }

    showScreen(screenId) {
        document.body.classList.remove('flash-correct', 'flash-wrong');
        document.querySelectorAll('.screen').forEach(screen =>  screen.classList.remove('active'));
        document.getElementById(screenId).classList.add('active');
        this.currentScreen = screenId;
    }

    showToast(message, type = 'info') {
        const toastEl = document.getElementById('toast');
        const toastBody = toastEl.querySelector('.toast-body');
        toastBody.textContent = message;

        toastEl.classList.remove('bg-success', 'bg-danger', 'bg-warning', 'bg-info', 'text-bg-success', 'text-bg-danger', 'text-bg-warning', 'text-bg-info');
        switch (type) {
            case 'success':
                toastEl.classList.add('text-bg-success');
                break;
            case 'error':
                toastEl.classList.add('text-bg-danger');
                break;
            case 'warning':
                toastEl.classList.add('text-bg-warning');
                break;
            default:
                toastEl.classList.add('text-bg-secondary');
        }

        const toast = new bootstrap.Toast(toastEl, { delay: 3000 });
        toast.show();
    }

    // Server discovery
    async discoverServers() {
        const serverList = document.getElementById('server-list');
        serverList.innerHTML = '<p class="text-secondary text-center py-3"><span class="spinner-border spinner-border-sm me-2"></span>Recherche de serveurs...</p>';

        window.quiznet.onServerDiscovered((server) => {
            const placeholder = serverList.querySelector('p');
            if (placeholder) {
                placeholder.remove();
            }

            if (serverList.querySelector(`[data-ip="${server.ip}"][data-port="${server.port}"]`)) {
                return;
            }

            const serverItem = document.createElement('div');
            serverItem.className = 'server-item';
            serverItem.dataset.ip = server.ip;
            serverItem.dataset.port = server.port;
            serverItem.innerHTML = `
                <div class="server-info">
                    <span class="server-name">${server.name || 'QuizNet Server'}</span>
                    <span class="server-ip">${server.ip}:${server.port}</span>
                </div>
            `;
            serverItem.addEventListener('click', () => this.connectToServer(server.ip, server.port));
            serverList.appendChild(serverItem);
        });

        try {
            const servers = await window.quiznet.discoverServers();
            if (servers.length === 0) 
                serverList.innerHTML = '<p class="text-secondary text-center py-3">Aucun serveur trouvé</p>';
        } catch (error) {
            serverList.innerHTML = '<p class="text-secondary text-center py-3">Erreur lors de la recherche</p>';
        }
    }

    async connectManual() {
        const ip = document.getElementById('server-ip').value;
        const port = parseInt(document.getElementById('server-port').value);
        await this.connectToServer(ip, port);
    }

    async connectToServer(ip, port) {
        try {
            await window.quiznet.connectServer(ip, port);
            this.showToast('Connecté au serveur', 'success');
            this.showScreen('login-screen');
        } catch (error) {
            this.showToast('Impossible de se connecter', 'error');
        }
    }

    async disconnectServer() {
        await window.quiznet.disconnectServer();
        this.showScreen('connection-screen');
        this.discoverServers();
    }

    switchTab(tabId) {
        document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
        document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
        document.querySelector(`[data-tab="${tabId}"]`).classList.add('active');
        document.getElementById(`${tabId}-tab`).classList.add('active');
    }

    async login() {
        const pseudo = document.getElementById('login-pseudo').value.trim();
        const password = document.getElementById('login-password').value;

        if (!pseudo || !password) {
            this.showToast('Veuillez remplir tous les champs', 'error');
            return;
        }

        try {
            await window.quiznet.sendRequest('POST', 'player/login', { pseudo, password });
        } catch (error) {
            this.showToast('Connexion au serveur perdue', 'error');
            this.showScreen('connection-screen');
            this.discoverServers();
        }
    }

    async register() {
        const pseudo = document.getElementById('register-pseudo').value.trim();
        const password = document.getElementById('register-password').value;

        if (!pseudo || !password) {
            this.showToast('Veuillez remplir tous les champs', 'error');
            return;
        }

        try {
            await window.quiznet.sendRequest('POST', 'player/register', { pseudo, password });
        } catch (error) {
            this.showToast('Connexion au serveur perdue', 'error');
            this.showScreen('connection-screen');
            this.discoverServers();
        }
    }

    logout() {
        this.pseudo = '';
        this.showScreen('login-screen');
    }

    // Lobby
    async loadThemes() {
        await window.quiznet.sendRequest('GET', 'themes/list');
    }

    async refreshSessions() {
        await window.quiznet.sendRequest('GET', 'sessions/list');
    }

    async createSession() {
        const name = document.getElementById('session-name').value.trim() || 'Session de ' + this.pseudo;
        const themeCheckboxes = document.querySelectorAll('#themes-list input:checked');
        const themeIds = Array.from(themeCheckboxes).map(cb => parseInt(cb.value));

        if (themeIds.length === 0) {
            this.showToast('Sélectionnez au moins un thème', 'error');
            return;
        }

        const difficulty = document.getElementById('session-difficulty').value;
        const nbQuestions = parseInt(document.getElementById('session-questions').value);
        const timeLimit = parseInt(document.getElementById('session-time').value);
        const mode = document.getElementById('session-mode').value;
        const maxPlayers = parseInt(document.getElementById('session-max').value);

        this.sessionMode = mode;

        const requestData = {
            name,
            themeIds,
            difficulty,
            nbQuestions,
            timeLimit,
            mode,
            maxPlayers
        };

        if (mode === 'battle') {
            requestData.lives = parseInt(document.getElementById('session-lives').value);
        }

        await window.quiznet.sendRequest('POST', 'session/create', requestData);
    }

    async joinSession(sessionId) {
        await window.quiznet.sendRequest('POST', 'session/join', { sessionId });
    }

    async startSession() {
        await window.quiznet.sendRequest('POST', 'session/start');
    }

    async leaveSession() {
        this.sessionId = null;
        this.showScreen('lobby-screen');
        this.refreshSessions();
    }

    backToLobby() {
        this.sessionId = null;
        this.score = 0;
        this.lives = 4;
        this.jokers = { fifty: 1, skip: 1 };
        this.showScreen('lobby-screen');
        this.refreshSessions();
    }

    startTimer(timeLimit) {
        this.currentTime = timeLimit;
        this.questionStartTime = Date.now();
        const timerEl = document.getElementById('timer');

        if (this.timerInterval) {
            clearInterval(this.timerInterval);
        }

        timerEl.textContent = this.currentTime;
        timerEl.className = 'timer fs-3 fw-bold text-info';

        this.timerInterval = setInterval(() => {
            this.currentTime--;
            timerEl.textContent = this.currentTime;

            if (this.currentTime <= 10) {
                timerEl.classList.add('warning');
            }
            if (this.currentTime <= 5) {
                timerEl.classList.remove('warning');
                timerEl.classList.add('danger');
            }

            if (this.currentTime <= 0) {
                clearInterval(this.timerInterval);
                if (!this.hasAnswered) {
                    this.submitAnswer(null);
                }
            }
        }, 1000);
    }

    stopTimer() {
        if (this.timerInterval) {
            clearInterval(this.timerInterval);
            this.timerInterval = null;
        }
    }

    getResponseTime() {
        return (Date.now() - this.questionStartTime) / 1000;
    }

    async submitAnswer(answer) {
        if (this.hasAnswered) return;
        this.hasAnswered = true;

        const responseTime = this.getResponseTime();

        document.querySelectorAll('.answer-btn').forEach(btn => {
            btn.disabled = true;
        });

        await window.quiznet.sendRequest('POST', 'question/answer', {
            answer,
            responseTime
        });
    }

    submitTextAnswer() {
        const answer = document.getElementById('text-answer').value.trim();
        if (answer) {
            this.submitAnswer(answer);
        }
    }

    async useJoker(type) {
        await window.quiznet.sendRequest('POST', 'joker/use', { type });
    }

    updateJokerButtons() {
        const fiftyBtn = document.getElementById('use-fifty');
        const skipBtn = document.getElementById('use-skip');

        if (this.jokers.fifty <= 0) {
            fiftyBtn.disabled = true;
            fiftyBtn.classList.add('used');
        }

        if (this.jokers.skip <= 0) {
            skipBtn.disabled = true;
            skipBtn.classList.add('used');
        }
    }

    updateGameInfo() {
        const scoreEl = document.getElementById('game-score');
        scoreEl.textContent = `Score: ${this.score}`;

        if (this.sessionMode === 'battle') {
            const livesEl = document.getElementById('game-lives');
            livesEl.classList.remove('hidden');
            livesEl.innerHTML = 'Vies: ' + '❤️'.repeat(this.lives);
        }
    }

    handleServerMessage(data) {
        console.log('Server message:', data);

        switch (data.action) {
            case 'player/register':
                this.handleRegisterResponse(data);
                break;
            case 'player/login':
                this.handleLoginResponse(data);
                break;
            case 'themes/list':
                this.handleThemesList(data);
                break;
            case 'sessions/list':
                this.handleSessionsList(data);
                break;
            case 'session/create':
            case 'session/join':
                this.handleSessionJoin(data);
                break;
            case 'session/player/joined':
                this.handlePlayerJoined(data);
                break;
            case 'session/player/left':
                this.handlePlayerLeft(data);
                break;
            case 'session/started':
                this.handleSessionStarted(data);
                break;
            case 'question/new':
                this.handleNewQuestion(data);
                break;
            case 'question/answer':
                // Answer acknowledgment
                break;
            case 'question/results':
                this.handleQuestionResults(data);
                break;
            case 'session/player/eliminated':
                this.handlePlayerEliminated(data);
                break;
            case 'session/finished':
                this.handleSessionFinished(data);
                break;
            case 'joker/use':
                this.handleJokerResponse(data);
                break;
            default:
                console.log('Unknown action:', data.action);
        }
    }

    handleRegisterResponse(data) {
        if (data.statut === '201') {
            this.showToast('Inscription réussie ! Connectez-vous maintenant.', 'success');
            this.switchTab('login');
        } else {
            this.showToast(data.message || 'Erreur lors de l\'inscription', 'error');
        }
    }

    handleLoginResponse(data) {
        if (data.statut === '200') {
            this.pseudo = document.getElementById('login-pseudo').value.trim();
            document.getElementById('user-pseudo').textContent = this.pseudo;
            this.showToast('Connexion réussie', 'success');
            this.showScreen('lobby-screen');
            this.loadThemes();
            this.refreshSessions();
        } else {
            this.showToast(data.message || 'Identifiants invalides', 'error');
        }
    }

    handleThemesList(data) {
        if (data.themes) {
            this.themes = data.themes;
            const themesContainer = document.getElementById('themes-list');
            themesContainer.innerHTML = data.themes.map(theme => `
                <label class="theme-item">
                    <input type="checkbox" value="${theme.id}" checked>
                    <span>${theme.name}</span>
                </label>
            `).join('');
        }
    }

    handleSessionsList(data) {
        const sessionsContainer = document.getElementById('sessions-list');

        if (!data.sessions || data.sessions.length === 0) {
            sessionsContainer.innerHTML = '<p class="text-secondary text-center py-4">Aucune session disponible</p>';
            return;
        }

        sessionsContainer.innerHTML = data.sessions.map(session => `
            <div class="session-card" data-id="${session.id}">
                <h6>${session.name}</h6>
                <div class="session-details">
                    <span class="badge ${session.mode === 'battle' ? 'bg-danger' : 'bg-primary'}">${session.mode === 'battle' ? 'Battle' : 'Solo'}</span>
                    <span>${session.themeNames ? session.themeNames.join(', ') : ''}</span>
                    <span>${session.nbQuestions} questions</span>
                    <span>${session.timeLimit}s/question</span>
                    <span>${session.nbPlayers}/${session.maxPlayers} joueurs</span>
                </div>
            </div>
        `).join('');

        sessionsContainer.querySelectorAll('.session-card').forEach(card => {
            card.addEventListener('click', () => {
                this.joinSession(parseInt(card.dataset.id));
            });
        });
    }

    handleSessionJoin(data) {
        if (data.statut === '201' || data.statut === '200') {
            this.sessionId = data.sessionId;
            this.isCreator = data.isCreator;
            if (data.mode) {
                this.sessionMode = data.mode;
            } else if (!this.sessionMode) {
                this.sessionMode = 'solo';
            }

            if (data.lives) {
                this.lives = data.lives;
            }
            if (data.jokers) {
                this.jokers = data.jokers;
            }

            if (!data.players) {
                data.players = [this.pseudo];
            }

            if (!data.name) {
                data.name = 'Session';
            }

            this.showScreen('waiting-screen');
            this.updateWaitingRoom(data);
        } else {
            this.showToast(data.message || 'Impossible de rejoindre la session', 'error');
        }
    }

    updateWaitingRoom(data) {
        document.getElementById('waiting-mode').textContent = this.sessionMode === 'battle' ? 'Battle (Vies)' : 'Solo (Score)';

        const playersContainer = document.getElementById('waiting-players');
        if (data.players) {
            playersContainer.innerHTML = data.players.map(pseudo => `
                <div class="player-card ${pseudo === this.pseudo ? 'self' : ''}">
                    <span class="player-name">${pseudo}</span>
                </div>
            `).join('');
        }

        const startBtn = document.getElementById('start-session');
        startBtn.disabled = !this.isCreator || (data.players && data.players.length < 2);

        document.getElementById('joker-fifty-count').textContent = this.jokers.fifty;
        document.getElementById('joker-skip-count').textContent = this.jokers.skip;

        if (this.sessionMode === 'battle') {
            document.getElementById('lives-info').classList.remove('hidden');
            document.getElementById('lives-display').textContent = '❤️ '.repeat(this.lives);
        } else {
            document.getElementById('lives-info').classList.add('hidden');
        }
    }

    handlePlayerJoined(data) {
        const playersContainer = document.getElementById('waiting-players');
        const playerCard = document.createElement('div');
        playerCard.className = 'player-card';
        playerCard.innerHTML = `<span class="player-name">${data.pseudo}</span>`;
        playersContainer.appendChild(playerCard);
        if (this.isCreator && data.nbPlayers >= 2) 
            document.getElementById('start-session').disabled = false;
        this.showToast(`${data.pseudo} a rejoint la session`, 'info');
    }

    handlePlayerLeft(data) {
        this.showToast(`${data.pseudo} a quitté la session`, 'info');
    }

    handleSessionStarted(data) {
        this.showToast('La partie commence !', 'success');
        this.score = 0;
        this.updateGameInfo();
    }

    handleNewQuestion(data) {
        this.showScreen('game-screen');
        this.hasAnswered = false;
        this.removedAnswers = [];
        this.currentQuestion = data;

        document.getElementById('question-num').textContent = data.questionNum;
        document.getElementById('question-total').textContent = data.totalQuestions;
        document.getElementById('question-type').textContent = data.type.toUpperCase();

        const difficultyEl = document.getElementById('question-difficulty');
        difficultyEl.textContent = data.difficulty.charAt(0).toUpperCase() + data.difficulty.slice(1);
        difficultyEl.className = `badge bg-${data.difficulty}`;

        document.getElementById('question-text').textContent = data.question;

        const fiftyBtn = document.getElementById('use-fifty');
        const skipBtn = document.getElementById('use-skip');
        fiftyBtn.disabled = this.jokers.fifty <= 0;
        skipBtn.disabled = this.jokers.skip <= 0;

        const answersContainer = document.getElementById('answers-container');
        const textContainer = document.getElementById('text-answer-container');
        const boolContainer = document.getElementById('boolean-container');

        answersContainer.classList.add('hidden');
        textContainer.classList.add('hidden');
        boolContainer.classList.add('hidden');

        if (data.type === 'qcm') {
            answersContainer.classList.remove('hidden');
            answersContainer.innerHTML = data.answers.map((answer, index) => `
                <div class="col-6">
                    <button class="answer-btn" data-index="${index}">${answer}</button>
                </div>
            `).join('');

            answersContainer.querySelectorAll('.answer-btn').forEach(btn => {
                btn.addEventListener('click', () => {
                    if (!this.hasAnswered) {
                        answersContainer.querySelectorAll('.answer-btn').forEach(b => b.classList.remove('selected'));
                        btn.classList.add('selected');
                        this.submitAnswer(parseInt(btn.dataset.index));
                    }
                });
            });

            fiftyBtn.disabled = false && this.jokers.fifty > 0;
        } else if (data.type === 'boolean') {
            boolContainer.classList.remove('hidden');

            boolContainer.querySelectorAll('.answer-btn').forEach(btn => {
                btn.classList.remove('selected');
                btn.disabled = false;
                btn.onclick = () => {
                    if (!this.hasAnswered) {
                        boolContainer.querySelectorAll('.answer-btn').forEach(b => b.classList.remove('selected'));
                        btn.classList.add('selected');
                        this.submitAnswer(btn.dataset.answer === 'true');
                    }
                };
            });

            fiftyBtn.disabled = true;
        } else if (data.type === 'text') {
            textContainer.classList.remove('hidden');
            document.getElementById('text-answer').value = '';
            document.getElementById('text-answer').focus();

            fiftyBtn.disabled = true;
        }

        this.startTimer(data.timeLimit);
        this.updateGameInfo();
    }

    handleQuestionResults(data) {
        this.stopTimer();
        this.showScreen('results-screen');

        const myResult = data.results.find(r => r.pseudo === this.pseudo);
        const isCorrect = myResult ? myResult.correct : false;
        
        document.body.classList.add(isCorrect ? 'flash-correct' : 'flash-wrong');

        let correctAnswerText = data.correctAnswer;
        if (this.currentQuestion) {
            if (this.currentQuestion.type === 'boolean') {
                correctAnswerText = data.correctAnswer ? 'Vrai' : 'Faux';
            } else if (this.currentQuestion.type === 'qcm' && typeof data.correctAnswer === 'number') {
                correctAnswerText = this.currentQuestion.answers[data.correctAnswer];
            }
        }

        const correctEl = document.getElementById('correct-answer');
        correctEl.innerHTML = `
            <p class="text-secondary mb-1">Bonne réponse</p>
            <h3 class="mb-0">${correctAnswerText}</h3>
        `;

        const explanationEl = document.getElementById('explanation');
        if (data.explanation) {
            explanationEl.textContent = data.explanation;
            explanationEl.classList.remove('hidden');
        } else {
            explanationEl.classList.add('hidden');
        }

        const resultsEl = document.getElementById('question-results');
        resultsEl.innerHTML = data.results.map(result => {
            if (result.pseudo === this.pseudo) {
                this.score = result.totalScore;
                if (result.lives !== undefined) 
                    this.lives = result.lives;
            }

            return `
                <div class="result-item">
                    <span class="player-name">${result.pseudo}</span>
                    <span class="${result.correct ? 'result-correct' : 'result-wrong'}">
                        ${result.correct ? 'Correct' : 'Incorrect'}
                    </span>
                    <span class="points">+${result.points}</span>
                </div>
            `;
        }).join('');

        if (data.lastPlayer) 
            this.showToast(`${data.lastPlayer} était le plus lent !`, 'warning');
    }

    handlePlayerEliminated(data) {
        this.showToast(`${data.pseudo} a été éliminé !`, 'error');
    }

    handleSessionFinished(data) {
        this.stopTimer();
        this.showScreen('final-screen');

        const winnerBanner = document.getElementById('winner-banner');
        if (data.mode === 'battle' && data.winner) {
            winnerBanner.textContent = `🏆 ${data.winner} remporte la partie !`;
            winnerBanner.classList.remove('hidden');
        } else {
            winnerBanner.classList.add('hidden');
        }

        const rankingEl = document.getElementById('final-ranking');
        rankingEl.innerHTML = data.ranking.map((player, index) => {
            let positionClass = '';
            let positionIcon = `#${player.rank}`;

            if (index === 0) {
                positionClass = 'gold';
                positionIcon = '🥇';
            } else if (index === 1) {
                positionClass = 'silver';
                positionIcon = '🥈';
            } else if (index === 2) {
                positionClass = 'bronze';
                positionIcon = '🥉';
            }

            let stats = `${player.correctAnswers} bonnes réponses`;
            if (player.lives !== undefined) 
                stats += ` • ${player.lives} ❤️ restantes`;
            if (player.eliminatedAt) 
                stats += ` • Éliminé à la question ${player.eliminatedAt}`;
            

            return `
                <div class="rank-item ${index === 0 ? 'first' : ''}">
                    <span class="rank-position ${positionClass}">${positionIcon}</span>
                    <div class="rank-info">
                        <div class="rank-name">${player.pseudo}</div>
                        <div class="rank-stats">${stats}</div>
                    </div>
                    <span class="rank-score">${player.score} pts</span>
                </div>
            `;
        }).join('');
    }

    handleJokerResponse(data) {
        if (data.statut === '200') {
            if (data.jokers) {
                this.jokers = data.jokers;
                this.updateJokerButtons();
            }

            if (data.remainingAnswers) {
                const answersContainer = document.getElementById('answers-container');
                const buttons = answersContainer.querySelectorAll('.answer-btn');

                buttons.forEach(btn => {
                    if (!data.remainingAnswers.includes(btn.textContent)) {
                        btn.classList.add('removed');
                        this.removedAnswers.push(parseInt(btn.dataset.index));
                    }
                });

                this.showToast('Joker 50/50 utilisé !', 'success');
            } else if (data.message === 'question skipped') {
                this.showToast('Question passée !', 'success');
                this.hasAnswered = true;
            }
        } else {
            this.showToast(data.message || 'Joker non disponible', 'error');
        }
    }

    handleConnectionError(error) {
        this.showToast('Erreur de connexion: ' + error, 'error');
        this.showScreen('connection-screen');
    }

    handleConnectionClosed() {
        this.showToast('Connexion au serveur perdue', 'error');
        this.showScreen('connection-screen');
        this.discoverServers();
    }
}

document.addEventListener('DOMContentLoaded', () => {
    window.client = new QuizNetClient();
});
