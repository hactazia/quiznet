# QuizNet

## Prerequisites

### Server
- GCC compiler
- Make
- For Windows cross-compilation on Linux: `mingw-w64`

### Client
- Node.js (v18 or higher)
- npm

## Building

### Server

```bash
cd server
make                    # Current platform    
make PLATFORM=linux     # For Linux
make PLATFORM=windows   # For Windows
```

### Client

Install dependencies:
```bash
cd client
npm install
```

Build for your platform:
```bash
npm run build           # Current platform
npm run build:win       # Windows
npm run build:linux     # Linux
```

Built applications will be in `client/dist/`.

## Running

### Server

#### Linux
```bash
cd server
./quiznet_server
```

#### Windows
```bash
cd server
quiznet_server.exe
```

### Client

Development mode:
```bash
cd client
npm start
```

Or run the built application from `client/dist/`.