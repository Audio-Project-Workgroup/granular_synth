import torch
import torch.onnx
import torch.nn as nn
import torch.nn.functional as F

import numpy as np

class EmbedNet(nn.Module):
    def __init__(self):
        super(EmbedNet, self).__init__()
        self.input_size = 2400
        self.embed_size = 100
        self.internal_width = 200
        self.in_channels = 2
        self.ker_size = 80
        self.stride = 16

        self.layers = nn.Sequential(
            nn.Conv1d(
                self.in_channels, self.internal_width, kernel_size=self.ker_size, stride=self.stride
            ),
            nn.BatchNorm1d(self.internal_width),
            nn.MaxPool1d(4),
            #
            nn.Conv1d(self.internal_width, self.internal_width, kernel_size=3),
            nn.BatchNorm1d(self.internal_width),
            nn.MaxPool1d(4),
            #
            nn.Conv1d(self.internal_width, 2 * self.internal_width, kernel_size=3),
            nn.BatchNorm1d(2 * self.internal_width),
            nn.MaxPool1d(4),
        )
        self.full_connect = nn.Linear(2 * self.internal_width, self.embed_size)

    def forward(self, X: torch.Tensor):
        x = self.layers(X)
        #x = F.avg_pool1d(x, x.shape[-1])
        x = F.adaptive_avg_pool1d(x, 1)
        x = x.permute(0, 2, 1)
        x = self.full_connect(x)
        return x

model_input = torch.randn(1, 2, 2400)
print(model_input)

model = EmbedNet()
model.load_state_dict(torch.load('../data/test_model.pth'))
model.eval()

torch.onnx.export(model, model_input, '../data/test_model.onnx')

device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
input_tensor = model_input.to(device)

with torch.no_grad():
    output_tensor = model(input_tensor)
print(output_tensor)
    
input_numpy = input_tensor.cpu().numpy().astype(np.float32)
input_numpy.tofile('../data/test_input.data')
output_numpy = output_tensor.cpu().numpy().astype(np.float32)
output_numpy.tofile('../data/test_output.data')
    
