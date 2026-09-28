import asyncio
import csv
import logging
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.staticfiles import StaticFiles

logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(name)s - %(levelname)s - %(message)s")
logger = logging.getLogger("VyasaBackend")

# MODULE 1: WebSocket Connection Manager

class WebSocketManager:
    """Handles active WebSocket connections and asynchronous broadcasting."""
    
    def __init__(self):
        self.active_connections: list[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)
        logger.info(f"Client connected. Active sessions: {len(self.active_connections)}")

    def disconnect(self, websocket: WebSocket):
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)
            logger.info(f"Client disconnected. Active sessions: {len(self.active_connections)}")

    async def broadcast(self, message: str):
        """Broadcasts a payload to all connected clients safely."""
        for connection in list(self.active_connections):
            try:
                await connection.send_text(message)
            except Exception as e:
                logger.warning(f"Failed to send message, dropping client. Error: {e}")
                self.disconnect(connection)


# MODULE 2: Telemetry Provider 

class MockTelemetryProvider:
    """Simulates a hardware data stream (RTK GPS) by yielding NMEA strings asynchronously."""
    
    def __init__(self, manager: WebSocketManager, file_path: str = "mock_gps_data.csv", rate_hz: int = 10):
        self.manager = manager
        self.file_path = file_path
        self.delay = 1.0 / rate_hz  # Converts 10Hz to a 0.1s delay

    async def stream_data(self):
        """Non-blocking background task that streams NMEA strings to the WebSocket manager."""
        logger.info(f"Initializing telemetry stream from '{self.file_path}' at {1.0/self.delay}Hz")
        
        while True:
            try:
                with open(self.file_path, "r") as file:
                    reader = csv.DictReader(file)
                    for row in reader:
                        if "GGA_String" in row:
                            await self.manager.broadcast(row["GGA_String"])
                        
                        # Yield control back to the asyncio event loop (simulates hardware timing)
                        await asyncio.sleep(self.delay) 
            except FileNotFoundError:
                logger.error(f"Dataset '{self.file_path}' missing! Please run generate_route.py first.")
                await asyncio.sleep(5)  



# MODULE 3: FastAPI Application & Routing

app = FastAPI(title="Project Drishti API by Team Vyasa", version="1.0.0")
ws_manager = WebSocketManager()
mock_provider = MockTelemetryProvider(manager=ws_manager)

@app.on_event("startup")
async def startup_event():
    """Bootstraps background worker tasks when the server initializes."""
    logger.info("Bootstrapping background telemetry workers...")
    asyncio.create_task(mock_provider.stream_data())

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """Full-duplex WebSocket endpoint for real-time telemetry (IMU & GPS)."""
    await ws_manager.connect(websocket)
    try:
        while True:
            msg = await websocket.receive_text()
            await ws_manager.broadcast(msg)
    except WebSocketDisconnect:
        ws_manager.disconnect(websocket)

app.mount("/", StaticFiles(directory=".", html=True), name="static")
