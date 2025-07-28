import numpy as np
import matplotlib.pyplot as plt

# Usa genfromtxt com skip_header e encoding explícito
data = np.genfromtxt('mpu_data.csv', delimiter=',', skip_header=1)

# Verifica se os dados foram carregados corretamente
if data.ndim != 2 or data.shape[1] < 7:
    raise ValueError("Erro ao ler o arquivo CSV: verifique o conteúdo e o número de colunas.")

# Separar as colunas
amostra     = data[:, 0]
accel_x     = data[:, 1]
accel_y     = data[:, 2]
accel_z     = data[:, 3]
giro_x      = data[:, 4]
giro_y      = data[:, 5]
giro_z      = data[:, 6]

# Plot dos dados do acelerômetro
plt.figure(figsize=(10, 5))
plt.plot(amostra, accel_x, 'r-', label='Accel X')
plt.plot(amostra, accel_y, 'g-', label='Accel Y')
plt.plot(amostra, accel_z, 'b-', label='Accel Z')
plt.title('Acelerômetro - Eixos X, Y, Z')
plt.xlabel('Número da Amostra')
plt.ylabel('Aceleração (g)')
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.show()

# Plot dos dados do giroscópio
plt.figure(figsize=(10, 5))
plt.plot(amostra, giro_x, 'r--', label='Giro X')
plt.plot(amostra, giro_y, 'g--', label='Giro Y')
plt.plot(amostra, giro_z, 'b--', label='Giro Z')
plt.title('Giroscópio - Eixos X, Y, Z')
plt.xlabel('Número da Amostra')
plt.ylabel('Velocidade Angular (°/s)')
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.show()
