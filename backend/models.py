from dataclasses import dataclass
from datetime import datetime


@dataclass
class Task:
    id: int
    text: str
    priority: str        # 'normal' | 'urgent'
    status: str          # 'pending' | 'done'
    created_at: str      # stored as ISO string from SQLite
    time_display: str    # "14:30 16/05" pre-formatted

    def to_dict(self) -> dict:
        return {
            "id": self.id,
            "text": self.text,
            "priority": self.priority,
            "status": self.status,
            "time_display": self.time_display,
        }
