import pandas as pd
import chess
import numpy as np
import torch
from torch.utils.data import Dataset, DataLoader
import torch.optim as optim
import torch.nn as nn
from sklearn.model_selection import train_test_split

device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
print(f"Using device: {device}")


# Function to convert FEN to NNUE input format (12x64 one-hot encoding)
def fen_to_input(fen):
    board = chess.Board(fen)
    input_tensor = np.zeros((12, 64), dtype=np.float32)

    piece_map = board.piece_map()
    for square, piece in piece_map.items():
        piece_idx = (piece.color * 6) + (piece.piece_type - 1)  # Encode color & piece type
        input_tensor[piece_idx][square] = 1  # One-hot encode pieces

    return input_tensor.flatten()  # Flatten to (768,)

# Chess Dataset Class
class ChessDataset(Dataset):
    def __init__(self, csv_file, train=True, train_size=0.8):
        # Load CSV with header handling
        self.data = pd.read_csv(csv_file, names=["FEN", "Evaluation"], skiprows=1)  
        self.data["Evaluation"] = pd.to_numeric(self.data["Evaluation"], errors='coerce')
        self.data = self.data.dropna()
        self.data["Evaluation"] = self.data["Evaluation"].astype(np.float32) / 10000.0  
        train_data, test_data = train_test_split(self.data, train_size=train_size, random_state=42)
        self.data = train_data if train else test_data

    def __len__(self):
        return len(self.data)

    def __getitem__(self, idx):
        fen = self.data.iloc[idx]["FEN"]
        eval_score = self.data.iloc[idx]["Evaluation"]
        
        input_tensor = fen_to_input(fen)  # Convert FEN to tensor
        return torch.tensor(input_tensor, dtype=torch.float32), torch.tensor([eval_score], dtype=torch.float32)

# Load dataset
csv_file = "train_tiny.csv"  # Change to your filename
train_dataset = ChessDataset(csv_file, train=True)
test_dataset = ChessDataset(csv_file, train=False)

# Create DataLoaders
train_loader = DataLoader(train_dataset, batch_size=32, shuffle=True)
test_loader = DataLoader(test_dataset, batch_size=32, shuffle=False)

# Verify dataset sizes
print(f"Training samples: {len(train_dataset)}, Testing samples: {len(test_dataset)}")

# Define NNUE Model
class NNUE(nn.Module):
    def __init__(self):
        super(NNUE, self).__init__()
        self.fc1 = nn.Linear(768, 256)
        self.fc2 = nn.Linear(256, 128)
        self.fc3 = nn.Linear(128, 1)
        self.relu = nn.ReLU()

    def forward(self, x):
        x = self.relu(self.fc1(x))
        x = self.relu(self.fc2(x))
        return self.fc3(x)

# Initialize model, loss function, and optimizer
model = NNUE().to(device)
criterion = nn.MSELoss()
optimizer = optim.Adam(model.parameters(), lr=1e-4)

# Training Loop
num_epochs = 10
for epoch in range(num_epochs):
    total_train_loss = 0
    total_val_loss = 0
    model.train()

    for inputs, targets in train_loader:
        inputs, targets = inputs.to(device), targets.to(device)  # Move to GPU
        optimizer.zero_grad()
        outputs = model(inputs)
        loss = criterion(outputs, targets)
        loss.backward()
        optimizer.step()
        total_train_loss += loss.item()

    # Compute validation loss
    model.eval()
    with torch.no_grad():
        for inputs, targets in test_loader:
            inputs, targets = inputs.to(device), targets.to(device)  # Move to GPU
            outputs = model(inputs)
            val_loss = criterion(outputs, targets)
            total_val_loss += val_loss.item()

    print(f"Epoch {epoch+1}/{num_epochs}, Train Loss: {total_train_loss:.4f}, Val Loss: {total_val_loss:.4f}")
    print(f"Epoch {epoch+1}/{num_epochs}, Average Train Loss: {total_train_loss/len(train_loader):.4f}, Average Val Loss: {total_val_loss/len(test_loader):.4f}")

# Save model weights (Corrected)
for name, param in model.named_parameters():
    np.save(f"{name}.npy", param.cpu().detach().numpy().T)  # Transpose before saving

print("Model weights saved as .npy files!")


# Function to evaluate a given FEN string using the trained NNUE model
def evaluate_fen(fen, model):
    model.eval()  # Set the model to evaluation mode
    input_tensor = fen_to_input(fen)  # Convert FEN to input format
    input_tensor = torch.tensor(input_tensor, dtype=torch.float32).to(device)  # Move to GPU if available

    input_tensor = input_tensor.unsqueeze(0)  # Add batch dimension

    with torch.no_grad():
        output = model(input_tensor)  # Run inference
    return output.item()  # Convert to scalar

# Evaluate the given FEN position
fen = "8/3q1pp1/4p1k1/3pPnp1/3P4/6B1/3Q1PPP/1RR3K1 w - - 2 29"
eval_score = evaluate_fen(fen, model)
print(f"NNUE Evaluation Score for FEN '{fen}': {eval_score:.4f}")
